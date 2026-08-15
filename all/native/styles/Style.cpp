#include "Style.h"

namespace massif {

    Style::~Style() {
    }
    
    const Color& Style::getColor() const {
        return _color;
    }
    
    Style::Style(const Color& color) :
        _color(color)
    {
    }
    
}
