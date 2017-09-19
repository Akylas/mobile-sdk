#include "HTTPClientWinSockImpl.h"
#include "components/Exceptions.h"
#include "utils/Log.h"

#include <cstddef>
#include <chrono>
#include <limits>
#include <regex>

#include <boost/algorithm/string.hpp>

#include <utf8.h>

#include <windows.h>
#include <wrl.h>
#include <msxml6.h>
#include <windows.storage.streams.h>

namespace {

    static std::wstring to_wstring(const std::string& str) {
        std::wstring wstr;
        utf8::utf8to16(str.begin(), str.end(), std::back_inserter(wstr));
        return wstr;
    }

    static std::string to_string(const std::wstring& wstr) {
        std::string str;
        utf8::utf16to8(wstr.begin(), wstr.end(), std::back_inserter(str));
        return str;
    }

    class HTTPPostStream : public Microsoft::WRL::RuntimeClass<
        Microsoft::WRL::RuntimeClassFlags< Microsoft::WRL::RuntimeClassType::WinRtClassicComMix >,
        ISequentialStream>
    {
    public:
        HRESULT RuntimeClassInitialize(
            const void* pv,
            ULONG cb)
        {
            m_data = std::vector<unsigned char>(static_cast<const unsigned char*>(pv), static_cast<const unsigned char*>(pv) + cb);
            return S_OK;
        }

        virtual ~HTTPPostStream() {
        }

        STDMETHODIMP Read(
            __out_bcount_part(cb, *pcbRead) void *pv,
            ULONG cb,
            __out_opt  ULONG *pcbRead)
        {
            HRESULT hr = S_OK;

            ULONG read = 0;
            for (read = 0; read < cb; ++read, ++m_buffSeekIndex) {
                if (m_buffSeekIndex == m_data.size()) {
                    hr = S_FALSE;
                    break;
                }

                static_cast<unsigned char*>(pv)[read] = m_data[m_buffSeekIndex];
            }

            if (pcbRead != NULL) {
                *pcbRead = read;
            }

            return hr;
        }

        STDMETHODIMP Write(
            __in_bcount(cb)  const void *pv,
            ULONG cb,
            __out_opt  ULONG *pcbWritten)
        {
            return E_NOTIMPL;
        }

        STDMETHODIMP GetSize(_Out_ ULONGLONG *pullSize)
        {
            if (pullSize == NULL) {
                return E_INVALIDARG;
            }

            *pullSize = m_data.size() - m_buffSeekIndex;
            return S_OK;
        }

    private:
        std::size_t m_buffSeekIndex = 0;
        std::vector<unsigned char> m_data;
    };

    class HTTPCallbackProxy : public Microsoft::WRL::RuntimeClass<
        Microsoft::WRL::RuntimeClassFlags< Microsoft::WRL::RuntimeClassType::WinRtClassicComMix >,
        IXMLHTTPRequest3Callback>
    {
    public:
        typedef std::function<bool(int, const std::map<std::string, std::string>&)> HeadersFunc;
        typedef std::function<bool(const unsigned char*, size_t)> DataFunc;
        typedef std::function<void(bool)> FinishFunc;

        HRESULT RuntimeClassInitialize(
            HeadersFunc headersFn,
            DataFunc dataFn,
            FinishFunc finishFn)
        {
            _headersFn = headersFn;
            _dataFn = dataFn;
            _finishFn = finishFn;
            return S_OK;
        }

        virtual ~HTTPCallbackProxy() {
        }

        STDMETHODIMP OnRedirect(
            IXMLHTTPRequest2 *pXHR,
            const WCHAR *pwszRedirectUrl)
        {
            return S_OK;
        }

        STDMETHODIMP OnHeadersAvailable(
            IXMLHTTPRequest2 *pXHR,
            DWORD dwStatus,
            const WCHAR *pwszStatus)
        {
            WCHAR* pwszHeaders = nullptr;

            HRESULT hr = pXHR->GetAllResponseHeaders(&pwszHeaders);
            if (FAILED(hr)) {
                _finishFn(false);
                return E_ABORT;
            }

            std::vector<std::string> headersVector;
            boost::split(headersVector, to_string(pwszHeaders), boost::is_any_of("\r\n"));
            std::map<std::string, std::string> headers;
            for (const std::string& header : headersVector) {
                std::string::size_type pos = header.find(':');
                if (pos != std::string::npos) {
                    std::string key = boost::trim_copy(header.substr(0, pos));
                    std::string value = boost::trim_copy(header.substr(pos + 1));
                    headers[key] = value;
                }
            }

            if (!_headersFn(dwStatus, headers)) {
                _finishFn(false);
                return E_ABORT;
            }
            return S_OK;
        }

        STDMETHODIMP OnDataAvailable(
            IXMLHTTPRequest2 *pXHR,
            ISequentialStream *pResponseStream)
        {
            while (true) {
                unsigned char buf[4096];
                unsigned long bytesRead = 0;
                HRESULT hr = pResponseStream->Read(buf, sizeof(buf), &bytesRead);
                if (FAILED(hr)) {
                    _finishFn(false);
                    return E_ABORT;
                }

                if (!bytesRead) {
                    break;
                }

                if (!_dataFn(&buf[0], bytesRead)) {
                    _finishFn(false);
                    return E_ABORT;
                }
            }
            return S_OK;
        }

        STDMETHODIMP OnResponseReceived(
            IXMLHTTPRequest2 *pXHR,
            ISequentialStream *pResponseStream)
        {
            _finishFn(true);
            return S_OK;
        }

        STDMETHODIMP OnError(
            IXMLHTTPRequest2 *pXHR,
            HRESULT hrError)
        {
            _finishFn(false);
            return S_OK;
        }

        STDMETHODIMP OnClientCertificateRequested(
            IXMLHTTPRequest3 *pXHR,
            DWORD cIssuerList,
            const WCHAR **rgpwszIssuerList
        )
        {
            return E_NOTIMPL;
        }

        STDMETHODIMP OnServerCertificateReceived(
            IXMLHTTPRequest3 *pXHR,
            DWORD dwCertificateErrors,
            DWORD cServerCertificateChain,
            const XHR_CERT *rgServerCertificateChain
        )
        {
            return S_OK;
        }

    private:
        HeadersFunc _headersFn;
        DataFunc _dataFn;
        FinishFunc _finishFn;
    };

}

namespace carto {

    HTTPClient::WinSockImpl::WinSockImpl(bool log) :
        _log(log),
        _timeout(-1)
    {
    }

    void HTTPClient::WinSockImpl::setTimeout(int milliseconds) {
        _timeout = milliseconds;
    }

    bool HTTPClient::WinSockImpl::makeRequest(const HTTPClient::Request& request, HeadersFunc headersFn, DataFunc dataFn) const {
        MULTI_QI mqi = { 0 };

        mqi.hr = S_OK;
        mqi.pIID = &__uuidof(IXMLHTTPRequest2);

        HRESULT hr = CoCreateInstanceFromApp(CLSID_FreeThreadedXMLHTTP60,
            nullptr,
            CLSCTX_INPROC_SERVER,
            nullptr,
            1,
            &mqi);
        if (FAILED(hr) || FAILED(mqi.hr)) {
            throw NetworkException("Failed to create XMLHTTP object", request.url);
        }

        Microsoft::WRL::ComPtr<IXMLHTTPRequest3> httpRequest3;
        httpRequest3.Attach(static_cast<IXMLHTTPRequest3*>(mqi.pItf));

        HANDLE event = ::CreateEventExA(NULL, NULL, 0, EVENT_MODIFY_STATE | SYNCHRONIZE);
        if (!event) {
            throw NetworkException("Failed to create event", request.url);
        }
        std::shared_ptr<std::remove_pointer<::HANDLE>::type> resultEvent(event, &::CloseHandle);

        bool cancel = false;
        auto finishFn = [&](bool success) {
            cancel = !success;
            ::SetEvent(resultEvent.get());
        };
        
        Microsoft::WRL::ComPtr<HTTPCallbackProxy> callbackProxy;
        hr = Microsoft::WRL::MakeAndInitialize<HTTPCallbackProxy>(&callbackProxy, headersFn, dataFn, finishFn);
        if (FAILED(hr)) {
            throw NetworkException("Failed to initialize callback proxy", request.url);
        }

        httpRequest3->SetProperty(XHR_PROP_NO_DEFAULT_HEADERS, XHR_CRED_PROMPT_NONE);
        httpRequest3->SetProperty(XHR_PROP_NO_AUTH, XHR_AUTH_NONE);
        httpRequest3->SetProperty(XHR_PROP_NO_DEFAULT_HEADERS, FALSE);
        httpRequest3->SetProperty(XHR_PROP_QUERY_STRING_UTF8, TRUE);
        if (_timeout > 0) {
            httpRequest3->SetProperty(XHR_PROP_TIMEOUT, _timeout);
        }

        hr = httpRequest3->Open(to_wstring(request.method).c_str(),
            to_wstring(request.url).c_str(),
            callbackProxy.Get(),
            nullptr,
            nullptr,
            nullptr,
            nullptr);

        if (FAILED(hr)) {
            throw NetworkException("Failed to open HTTP connection", request.url);
        }

        for (auto it = request.headers.begin(); it != request.headers.end(); it++) {
            httpRequest3->SetRequestHeader(to_wstring(it->first).c_str(), to_wstring(it->second).c_str());
        }

        if (!request.contentType.empty()) {
            Microsoft::WRL::ComPtr<HTTPPostStream> postStream;
            hr = Microsoft::WRL::MakeAndInitialize<HTTPPostStream>(&postStream, request.body.data(), request.body.size());
            if (FAILED(hr)) {
                throw NetworkException("Failed to initialize HTTP POST stream", request.url);
            }

            hr = httpRequest3->Send(postStream.Get(), request.body.size());
        }
        else {
            hr = httpRequest3->Send(nullptr, 0);
        }

        if (FAILED(hr)) {
            throw NetworkException("Failed to send HTTP request", request.url);
        }

        ::WaitForSingleObjectEx(resultEvent.get(), INFINITE, FALSE);

        return !cancel;
    }

}
