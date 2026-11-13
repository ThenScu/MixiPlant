# MixiPlan local gateway

Gateway local nhận telemetry và ACK qua MQTT, lưu lịch sử vào SQLite, nhận dữ liệu webhook có token và phục vụ dashboard có tài khoản `admin`/`viewer`.

## Chạy từ CMD trong VS Code

Mở terminal tại thư mục gốc project (nơi có `platformio.ini`) rồi chạy:

```cmd
gateway\run.bat
```

Lần đầu script tự tạo Python virtual environment, cài packages và tạo `.env` cục bộ với thông tin đăng nhập ngẫu nhiên. Các lần chạy sau bỏ qua cài đặt nếu packages đã sẵn sàng. Mở `gateway/.env` để xem mật khẩu; file này được Git ignore. Nếu muốn cài lại/cập nhật packages, chạy `gateway\run.bat --install`.

Mở `http://127.0.0.1:8000`. Đăng nhập bằng `admin` hoặc `viewer` và mật khẩu tương ứng trong `.env`. Viewer chỉ xem; admin mới có nút gửi lệnh. Dashboard cần broker MQTT kết nối được và thiết bị đang publish đúng topic. Dữ liệu sẽ được lưu ở `gateway/data/mixiplan.sqlite3`.

## Demo đầu vào trên dashboard

Khối **Demo đầu vào** (chỉ admin) gửi lệnh `SOIL:<0-100>` / `RAIN:<0-100>` để ép độ ẩm đất và mưa khi demo, không cần chạm mạch Wokwi. Override tự hết hạn sau 2 phút; chỉnh núm tương ứng trên mạch lệch quá 5% cũng hủy override ngay (mạch luôn thắng). Nút **Trả về mạch** gửi `SOIL:-1` / `RAIN:-1` để hủy sớm. Telemetry backfill từ ring buffer của ESP32 được gateway tự bung về schema đầy đủ (`backfill=true`, kèm `device_time_ms`).

## Webhook nhận telemetry

Gửi POST JSON tới `http://127.0.0.1:8000/api/webhooks/telemetry` với header `X-Webhook-Token: <WEBHOOK_TOKEN>`. Endpoint chỉ nhận dữ liệu, không tự đẩy lệnh đến ESP32. Firmware hiện gửi telemetry bằng MQTT, vì vậy webhook chỉ có tác dụng khi một dịch vụ khác gọi endpoint.

## Ghi chú bảo mật

- MQTT mặc định dùng broker công cộng không mã hóa để demo. Không nối bơm thật hoặc gửi dữ liệu riêng tư qua cấu hình này.
- Muốn dùng broker riêng có TLS, cấu hình `MQTT_TLS=1`, port TLS, username/password, CA cert; firmware ESP32 cũng phải được cấu hình cùng broker và chứng thư.
- API hiện chỉ bind vào loopback. Truy cập từ Internet cần HTTPS, reverse proxy, quản lý secrets và firewall; không đổi host sang `0.0.0.0` để mở tùy tiện.
