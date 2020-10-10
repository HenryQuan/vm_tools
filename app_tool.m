//
//  AppTool.m
//  Injection
//
//  Created by Yiheng Quan on 26/9/20.
//

#import "app_tool.h"

@implementation AppTool

+(NSString *)getAppVersion
{
    return [[[NSBundle mainBundle] infoDictionary] objectForKey:@"CFBundleShortVersionString"];
}

+(unsigned long long)getBinarySize
{
    NSString *appPath = [[NSBundle mainBundle] resourcePath];
    NSString *appName = [[[NSBundle mainBundle] infoDictionary] objectForKey:@"CFBundleExecutable"];
    NSString *binaryPath = [NSString stringWithFormat:@"%@/%@", appPath, appName];
    NSLog(@"[APP_TOOL] Current app path is %@\n", appPath);
    
    // Read app binary size
    NSFileManager *fileManager = [NSFileManager defaultManager];
    return [[fileManager attributesOfItemAtPath:binaryPath error:nil] fileSize];
}

@end
