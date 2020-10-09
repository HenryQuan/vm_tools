//
//  app_tool.h
//  Injection
//
//  Created by Yiheng Quan on 26/9/20.
//

#ifndef APP_TOOL_H
#define APP_TOOL_H

#include <Foundation/Foundation.h>

@interface AppTool : NSObject

+(NSString *)getAppVersion;
+(unsigned long long)getBinarySize;

@end

#endif
