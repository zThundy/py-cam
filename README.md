# PyCam

PyCam is a self-hosted CCTV system designed around ESP32S3 cam modules.

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
- [Seeed Studio](https://www.seeedstudio.com/XIAO-ESP32S3-p-5627.html)
- [Aliexpress](https://www.aliexpress.com/item/1005005543738460.html)
- [Amazon.it](https://www.amazon.it/ESP32S3-2-4GHz-interfaccia-intelligenti-dispositivi-indossabili/dp/B0BYSB66S5?th=1)
- [Amazon.com](https://www.amazon.com/ESP32S3-2-4GHz-interfaccia-intelligenti-dispositivi-indossabili/dp/B0BYSB66S5?th=1)

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
