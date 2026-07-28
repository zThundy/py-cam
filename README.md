# PyCam

PyCam is a self-hosted CCTV system designed around **ESP32S3 SENSE cam** modules.

The project is very much a work in progress, so don't expect amazing code or super cool features...

The server is written in python and it will use "Multicast" to automatically discover ESP in the local network.

It is recommended that you connect the ESPs on a separate IoT network (if your router supports it

## Docker installation (recommended)

The docker-compose file will install everything for you. If you know how a docker-compose.yml and a dockerfile work, good for you :)

If not, then create a folder and copy the docker-file to it.

```yaml
services:
  app:
    container_name: esp32s3-cam-server
    image: zthundy/esp32s3-cam-server
    restart: unless-stopped

    network_mode: host

    user: "${UID}:${GID}"

    environment:
      LOGLEVEL: "INFO"
      LOGPATH: "logs"
      IMAGESPATH: "images"
      MAXIMAGES: 1200
      SERVICE_PORT: 4512
      MULTICASTGROUP: '239.1.2.3'
      MULTICASTPORT: 3344

    volumes:
      - images_data:/app/images
      - logs_data:/app/logs

volumes:
  images_data:
  logs_data:
```

Change the `environment` section with you preferred configurations.

Keep in mind that changing the ports will also impact the configuration in your ESP code, so chage them if you know what to do next since I will not bother to explain what to do.

***Important:*** since the docker installation is using `network_mode: host` for multicast support, remember to open the ports (only for local network) on the server you are installing the image to.

```yaml
    environment:
      LOGLEVEL: "INFO"
      LOGPATH: "logs"
      IMAGESPATH: "images"
      MAXIMAGES: 1200
      SERVICE_PORT: 4512
      MULTICASTGROUP: '239.1.2.3'
      MULTICASTPORT: 3344
```

## Manual installation

For the manual installation:
- Clone the repo
- Set the environment variables (or don't the server has defaults)
- Use python (3.10 or later)
- Run `python server.py`

## Flashing the firmware on ESP

Download the ArduinoIDE from [here](https://www.arduino.cc/en/software/).

Regarding the hardware, I have tested the software on these modules
- [Seeed Studio](https://www.seeedstudio.com/XIAO-ESP32S3-Sense-p-5639.html)
- [Aliexpress](https://www.aliexpress.us/item/3256805357906723.html?spm=a2g0o.detail.pcDetailTopMoreOtherSeller.1.281dUTArUTArwr&gps-id=pcDetailTopMoreOtherSeller&scm=1007.40050.354490.0&scm_id=1007.40050.354490.0&scm-url=1007.40050.354490.0&pvid=eb580d60-7d31-46eb-a886-2254db7bd82d&_t=gps-id%3ApcDetailTopMoreOtherSeller%2Cscm-url%3A1007.40050.354490.0%2Cpvid%3Aeb580d60-7d31-46eb-a886-2254db7bd82d%2Ctpp_buckets%3A668%232846%238116%232002&pdp_ext_f=%7B%22order%22%3A%22805%22%2C%22eval%22%3A%221%22%2C%22sceneId%22%3A%2230050%22%2C%22fromPage%22%3A%22recommend%22%7D&pdp_npi=6%40dis%21GBP%2117.70%2113.27%21%21%2122.85%2117.13%21%402103810f17852271911181967e0f32%2112000047978847658%21rec%21GB%21%21ABXZ%211%210%21n_tag%3A-29910%3Bd%3A72a4d18%3Bm03_new_user%3A-29895%3BpisId%3A5000000212851917&utparam-url=scene%3ApcDetailTopMoreOtherSeller%7Cquery_from%3A%7Cx_object_id%3A1005005544221475%7C_p_origin_prod%3A&gatewayAdapt=glo2usa4itemAdapt)
- [Amazon.it](https://www.amazon.it/ESP32S3-2-4GHz-interfaccia-intelligenti-dispositivi-indossabili/dp/B0C69FFVHH?th=1)
- [Amazon.com](https://www.amazon.com/ESP32S3-2-4GHz-interfaccia-intelligenti-dispositivi-indossabili/dp/B0C69FFVHH?th=1)

To flash, connect the ESP to your computer and make sure the board is recognized by the ArduinoIDE, if not you will have to download the esp32 board list by going into **File > Preferences > Additional boards** and add the following url
```
https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
```

Then select the XIAO_ESP32S3 from the list of available boards and everything should be ready to flash.

If you don't know how to flash an ESP32, good thing there are about a million tutorials online and this is not one of them.

Anyway, make sure to change the
```c#
const char* ssid = "XXXXXX";
const char* password = "XXXXX";
```
and *(only if you decided to be smart and changed them in the ENV section)*
```c#
IPAddress multicastIP(239, 1, 2, 3);
unsigned int multicastPort = 3344;
....
unsigned int servicePort = 4512;
```

## TODO

- https for secure website and connection
- wss same as above
- SQL cause storing jpeg on server is highly inefficient
- Something else i can't think of right now

## Contributing

Pull requests are welcome. For major changes, please open an issue first
to discuss what you would like to change.

## License

[GPL v3](https://choosealicense.com/licenses/gpl-3.0/)
