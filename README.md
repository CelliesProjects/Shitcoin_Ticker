# Shitcoin_Ticker

<img src="https://user-images.githubusercontent.com/24290108/148062158-45a9d093-6d68-4bf7-bff5-e8cd5f775aad.jpg" width="50%" alt="file overlay">

This is a simple coin ticker that connects to `api.kraken.com` to get the trade data. This is a public api so no account at kraken.com is needed.

Included is the public SSL key cypher block so the connection is secure until the SSL certificate becomes invalid.

The certificate will become invalid at `august 5 2022, 1:59:59 AM GMT+2`.

The program will not run with less than 1MB PSRAM. 

Set `Tools->Core Debug Level` to `info` to see messages and errors on the serial port.
