# Agent Memory Index

- [project_r4_pinmode_bug.md](project_r4_pinmode_bug.md) - Root cause: ARM global constructors run before hardware init, ShifterStr pinMode() silently fails on R4 WiFi
- [project_serial_status_bug.md](project_serial_status_bug.md) - Admin panel shows disconnected due to port.set() DTR/RTS error causing reconnect loop
- [project_auth_token_bug.md](project_auth_token_bug.md) - Scorer and admin pages use different localStorage keys for the same token, causing repeated prompts
