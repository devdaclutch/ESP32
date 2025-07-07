# Lab 7.1 – Fetch Weather from wttr.in

ESP32 makes an HTTP GET request to wttr.in and logs the temperature data.

## Notes

- No HTTPS: plain HTTP used.

  
 # Lab 7.2 – Post Sensor Data to Local Server

Reads local temp/humidity data and sends it via HTTP POST to a local server (on Pi4 or phone).

## Payload

```json

{
  "temperature": 23.7,
  "humidity": 48.2
}

```
## 🔸 Lab 7.3 – Combined Weather Reporting

```markdown
# Lab 7.3 – Local + Remote Weather Integration

1. ESP32 requests a location string from a server.
2. Queries `wttr.in` for that location's weather.
3. Combines it with onboard temp/humidity readings.
4. Posts the merged data back to the server.

## Tech Used

- HTTP GET + POST
- JSON formatting
- Dynamic sensor + API integration
