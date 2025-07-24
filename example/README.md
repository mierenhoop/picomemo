### Example instant messenger

Note: the code here is somewhat outdated, however it still works.

The [`im.c`](./example/im.c) example shows how additional
functionality can be used in combination with the library. The example
is not a feature complete instant messenger and for simplicity's sake
the code is full of hardcoded and spec deviating behaviour that should
not represent a serious XMPP client.

Run the im (instant messenger) example:

`$ make runim`

By default the localhost self-signed certificate is used. For a simple
test you can spin up prosody (`$ make start-prosody`) and run the echo
bot (`$ make start-omemo-bot`).

Compile the esp-idf version of the im:

```bash
$ cat > example/esp-im/config.h <<EOF
#define IM_WIFI_SSID "ssid"
#define IM_WIFI_PASS "password"
#define IM_SERVER_IP "192.168.1.2"
EOF
```

`$ make esp-im`

`$ ESP_DEV=/dev/ttyUSB0 make esp-upload`

`$ ESP_DEV=/dev/ttyUSB0 make esp-monitor`

### Demo of XMPP with OMEMO on an ESP32

https://github.com/user-attachments/assets/b01d9439-f30b-4062-8711-02cbf9599e67
