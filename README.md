# VM Tools

`app_tool` and `vm_tool` includes useful helper functions. Include both of them in your project to use them.

- NOTE: add `app_tool.m` in your compile list or it won't compile

## TODO

remove app_tool if no longer needed, \_dyld_get_image_vmaddr_slide(1) should be the end address

### Extras

```
xattr -cr /path/to/application.app
```

This fixes damaged file on Mac.
