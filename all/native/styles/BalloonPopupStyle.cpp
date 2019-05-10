#include "BalloonPopupStyle.h"
#include "graphics/Bitmap.h"

namespace carto {
    
    BalloonPopupStyle::BalloonPopupStyle(const Color& color,
                                         float attachAnchorPointX,
                                         float attachAnchorPointY,
                                         bool causesOverlap,
                                         bool hideIfOverlapped,
                                         float horizontalOffset,
                                         float verticalOffset,
                                         int placementPriority,
                                         bool scaleWithDPI,
                                         const std::shared_ptr<AnimationStyle>& animStyle,
                                         int cornerRadius,
                                         const Color& leftColor,
                                         const std::shared_ptr<Bitmap>& leftImage,
                                         const BalloonPopupMargins& leftMargins,
                                         const Color& rightColor,
                                         const std::shared_ptr<Bitmap>& rightImage,
                                         BalloonPopupMargins& rightMargins,
                                         const Color& titleColor,
                                         const std::string& titleFontName,
                                         const std::string& titleField,
                                         int titleFontSize,
                                         const BalloonPopupMargins& titleMargins,
                                         bool titleWrap,
                                         const Color& descColor,
                                         const std::string& descFontName,
                                         const std::string& descField,
                                         int descFontSize,
                                         const BalloonPopupMargins& descMargins,
                                         bool descWrap,
                                         const BalloonPopupMargins& buttonMargins,
                                         const Color& strokeColor,
                                         int strokeWidth,
                                         int triangleWidth,
                                         int triangleHeight) :
        PopupStyle(Color(0xFFFFFFFF),
                   attachAnchorPointX,
                   attachAnchorPointY,
                   causesOverlap,
                   hideIfOverlapped,
                   horizontalOffset,
                   verticalOffset,
                   placementPriority,
                   scaleWithDPI,
                   animStyle),
        _backgroundColor(color),
        _cornerRadius(cornerRadius),
        _leftColor(leftColor),
        _leftImage(leftImage),
        _leftMargins(leftMargins),
        _rightColor(rightColor),
        _rightImage(rightImage),
        _rightMargins(rightMargins),
        _titleColor(titleColor),
        _titleFontName(titleFontName),
        _titleField(titleField),
        _titleFontSize(titleFontSize),
        _titleMargins(titleMargins),
        _titleWrap(titleWrap),
        _descColor(descColor),
        _descFontName(descFontName),
        _descField(descField),
        _descFontSize(descFontSize),
        _descMargins(descMargins),
        _descWrap(descWrap),
        _buttonMargins(buttonMargins),
        _strokeColor(strokeColor),
        _strokeWidth(strokeWidth),
        _triangleWidth(triangleWidth),
        _triangleHeight(triangleHeight)
    {
    }

    BalloonPopupStyle::~BalloonPopupStyle() {
    }
        
    const Color& BalloonPopupStyle::getBackgroundColor() const {
        return _backgroundColor;
    }
        
    int BalloonPopupStyle::getCornerRadius() const {
        return _cornerRadius;
    }
        
    const Color& BalloonPopupStyle::getLeftColor() const {
        return _leftColor;
    }

    const std::shared_ptr<Bitmap>& BalloonPopupStyle::getLeftImage() const {
        return _leftImage;
    }

    const BalloonPopupMargins& BalloonPopupStyle::getLeftMargins() const {
        return _leftMargins;
    }

    const Color& BalloonPopupStyle::getRightColor() const {
        return _rightColor;
    }

    const std::shared_ptr<Bitmap>& BalloonPopupStyle::getRightImage() const {
        return _rightImage;
    }

    const BalloonPopupMargins& BalloonPopupStyle::getRightMargins() const {
        return _rightMargins;
    }

    const Color& BalloonPopupStyle::getTitleColor() const {
        return _titleColor;
    }

    const std::string& BalloonPopupStyle::getTitleFontName() const {
        return _titleFontName;
    }

    const std::string& BalloonPopupStyle::getTitleField() const {
        return _titleField;
    }

    int BalloonPopupStyle::getTitleFontSize() const {
        return _titleFontSize;
    }

    const BalloonPopupMargins& BalloonPopupStyle::getTitleMargins() const {
        return _titleMargins;
    }
        
    bool BalloonPopupStyle::isTitleWrap() const {
        return _titleWrap;
    }

    const Color& BalloonPopupStyle::getDescriptionColor() const {
        return _descColor;
    }

    const std::string& BalloonPopupStyle::getDescriptionFontName() const {
        return _descFontName;
    }

    const std::string& BalloonPopupStyle::getDescriptionField() const {
        return _descField;
    }

    int BalloonPopupStyle::getDescriptionFontSize() const {
        return _descFontSize;
    }

    const BalloonPopupMargins& BalloonPopupStyle::getDescriptionMargins() const {
        return _descMargins;
    }
        
    bool BalloonPopupStyle::isDescriptionWrap() const {
        return _descWrap;
    }

    const BalloonPopupMargins& BalloonPopupStyle::getButtonMargins() const {
        return _buttonMargins;
    }
        
    const Color& BalloonPopupStyle::getStrokeColor() const {
        return _strokeColor;
    }

    int BalloonPopupStyle::getStrokeWidth() const {
        return _strokeWidth;
    }

    int BalloonPopupStyle::getTriangleWidth() const {
        return _triangleWidth;
    }

    int BalloonPopupStyle::getTriangleHeight() const {
        return _triangleHeight;
    }

}
