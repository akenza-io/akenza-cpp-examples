# akenza-cpp-examples

A set of C++ examples for using the `akenza` APIs.

Also refer to the [product documentation](https://docs.akenza.io/), the documentation of
the [REST API](https://docs.api.akenza.io/).

## Installation

```
# install vcpkg
git clone https://github.com/microsoft/vcpkg.git $HOME/bin/vcpkg
$HOME/bin/vcpkg/bootstrap-vcpkg.sh
export VCPKG_ROOT=$HOME/bin/vcpkg
export PATH=$VCPKG_ROOT:$PATH

# install dependencies
cmake -S. -B.build -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake

# build
cmake --build .build
./bin/ak_mqtt --help
```

## MQTT Example

This example connects to akenza via MQTT with the [Eclipse Paho CPP Client](https://github.com/eclipse/paho.mqtt.cpp/)
using a JWT for [device authentication](https://docs.akenza.io/akenza.io/api-reference/device-security). After
connecting, the client subscribes to the downlink topic and messages are published to the device's MQTT uplink topic
at a rate of one message every 5 seconds.

To run this example,
first [create your credentials](https://docs.akenza.io/akenza.io/tutorials/using-device-credentials/creating-public-private-key-pairs)
and register your device in akenza.

In akenza:

- Create a new data flow with an MQTT device connector using `Device credentials` as the authentication method.
- Create a device with this data flow, select the algorithm `RSA-256 x.509` or `EC-256 x.509` and upload/paste the
  certificate.

After you have generated your credentials and created the device in akenza, compile and run this example with the
corresponding algorithm flag.

NOTE: always keep your private key secret.

### Using a Self-Signed RSA X509 Certificate

Generate the private key and certificate:

```
openssl req -x509 -nodes -newkey rsa:2048 -keyout ./keys/rsa_private.pem -out ./keys/rsa_cert.pem -subj "/CN=<deviceId>"
```

Run the example:

```
cmake --build .build
./bin/ak_mqtt \
    --device_id="<deviceId>" \
    --algorithm="RS256" \
    --private_key_file="../path/to/your_rsa_key"
```

A full example:

```
mkdir keys
openssl req -x509 -nodes -newkey rsa:2048 -keyout ./keys/rsa_private.pem -out ./keys/rsa_cert.pem -subj "/CN=965200347BC53111"
pbcopy < ./keys/rsa_cert.pem
# create the device in akenza and paste the certificate
./bin/ak_mqtt \
    --device_id="965200347BC53111" \
    --algorithm="RS256" \
    --private_key_file="./keys/rsa_private.pem"
```

### Using a Self-Signed EC X509 Certificate

Generate the private key and certificate:

```
openssl ecparam -genkey -name prime256v1 -noout -out ./keys/ec_private.pem
openssl req -x509 -new -key ./keys/ec_private.pem -out ./keys/ec_cert.pem -subj "/CN=<deviceId>"
```

Run the example:

```
cmake --build .build
./bin/ak_mqtt \
    --device_id="<deviceId>" \
    --algorithm="ES256" \
    --private_key_file="../path/to/your_ec_key"
```

A full example:

```
mkdir keys
openssl ecparam -genkey -name prime256v1 -noout -out ./keys/ec_private.pem
openssl req -x509 -new -key ./keys/ec_private.pem -out ./keys/ec_cert.pem -subj "/CN=D5C30777E0A6ED9B"
pbcopy < ./keys/ec_cert.pem
# create the device in akenza and paste the certificate
cmake --build .build
./bin/ak_mqtt \
    --device_id="D5C30777E0A6ED9B" \
    --algorithm="ES256" \
    --private_key_file="./keys/ec_private.pem"
```

More configuration options can be set with:

```
./bin/ak_mqtt --help

An example CLI for connecting to the akenza MQTT broker.
Usage:
  ak_mqtt [OPTION...]

  -s, --mqtt_hostname arg     MQTT hostname (default: mqtt.akenza.io)
  -p, --mqtt_port arg         MQTT port (default: 8883)
  -d, --device_id arg         The physical device id
  -u, --mqtt_username arg     MQTT username (only for uplink secret
                              authentication)
  -r, --mqtt_password arg     MQTT password (only for uplink secret
                              authentication)
  -a, --algorithm arg         Signing algorithm (ES256 or RS256) (default: ES256)
  -c, --audience arg          Audience (e.g. akenza.io) (default: akenza.io)
  -f, --private_key_file arg  Path to the private key
  -v, --verbose               Enable verbose output
  -h, --help                  Print usage
```

For private cloud customers, the audience has to be changed accordingly (e.g. `<customer>.akenza.io`).
