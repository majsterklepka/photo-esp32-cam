# photo-esp32-cam

ESP-IDF environment is required for build firmware, here [github.com/espressif/esp-idf](https://github.com/espressif/esp-idf "esp-idf"). First, you need to configure the project

```
idf.py menuconfig
```

this will set the correct version number and set the environment variables. To build, we give a cmd :
```
idf.py build
```

and to upload to ESP32-CAM you must give the command
```
idf.py -p /dev/your-port-name flash monitor
```

## Credits

Project is licensed under terms of GNU GPLv3 license. It uses other part code taken from [ESP32-CAM-driver](https://github.com/espressif/esp32-camera "driver") with own license Apache License Version 2.0, January 2004.

Have a fun, and share this project with friend, Be happy but always remember about the authors.

_SQ7EQE_

