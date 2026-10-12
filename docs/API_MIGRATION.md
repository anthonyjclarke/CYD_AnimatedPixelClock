# Runtime API migration — 2.0.0

State-changing routes now require POST. Their existing query/form parameters
still work. A GET to these routes returns 405 with `Allow: POST` and makes no
change:

| Route | Parameters/purpose |
| --- | --- |
| `/api/display/on`, `/api/display/off` | Display power |
| `/api/display/brightness` | `value=1..100` |
| `/api/mode/clock`, `/api/mode/auto`, `/api/mode/ambient` | Clock/screensaver mode |
| `/api/clock/style` | `id` clock style |
| `/api/reboot` | Restart |
| `/api/notify/dismiss` | Dismiss notification |
| `/api/ntptest` | `server` NTP probe |
| `/reset` | Factory reset |

`/save`, `/api/import`, `/api/rename` and `/api/notify` remain POST, with the same
origin checks. `/update` remains a multipart POST containing firmware.
`/api/info`, `/api/diagnostics`, `/api/status` and `/api/export` remain GET.

Examples for scripts:

```sh
curl -X POST 'http://pixelclock.local/api/clock/style?id=14'
curl -X POST 'http://pixelclock.local/api/display/brightness?value=70'
curl -X POST 'http://pixelclock.local/api/mode/auto'
```

For Home Assistant `rest_command` entries, set `method: POST`; the URLs and
parameters can stay the same. The device's own web UI has been updated.

Browser mutations must originate from the requested device authority. Foreign,
sibling or opaque origins are rejected with 403. Scripts and Home Assistant
without browser Origin/Fetch Metadata headers remain supported. Reverse proxies
should preserve the browser-facing Host header. This is a browser request guard,
not a replacement for network access control or authentication.

Exports use ArduinoJson serialization for all strings, have an attachment
filename and `Cache-Control: no-store`, and no longer grant wildcard CORS.
Foreign browser requests are refused; native GET clients remain supported.
JSON field names and the Phase 4 colour schema remain unchanged, with
`weatherLayout` added (0=current, 1=rows, 2=columns, 3=hourly, 4=tomorrow).

OTA requests without a file/multipart boundary return 400, failures return 500,
and foreign requests return 403. Restart follows a completed successful image
only. Aborted uploads clear request state; a later empty request cannot reuse an
earlier success.
