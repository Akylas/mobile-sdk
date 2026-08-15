/*
 * Copyright (c) 2016 CartoDB. All rights reserved.
 * Copying and using this code is allowed only according
 * to license terms, as given in https://cartodb.com/terms/
 */

#import "MSFRedrawRequestListener.h"
#import "ui/MapView.h"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"

@interface MSFMapRedrawRequestListener : MSFRedrawRequestListener

@property (weak, nonatomic) MSFGLKView* view;

-(id)initWithView:(MSFGLKView*)view;

@end

#pragma clang diagnostic pop
