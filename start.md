# Cách chạy MixiPlan

Tài liệu này hướng dẫn chạy mô hình ESP32 trên VS Code/Wokwi, sau đó nối MQTT và dashboard local trên Windows. Làm từng lệnh một, nhấn Enter sau mỗi dòng. Không dán nhiều lệnh liền nhau vào một dòng.

## 1. Cần cài trước

- **Visual Studio Code**.
- Extension **PlatformIO IDE** để build firmware.
- Extension **Wokwi Simulator** nếu muốn mô phỏng trong VS Code. Ảnh trước báo license Wokwi hết hạn; khi đó build vẫn có thể thành công nhưng simulator không chạy.
- **Python 3** để chạy gateway/dashboard. Khi cài Python, bật **Add Python to PATH**. Sau khi cài, mở terminal mới.
- Internet ở lần chạy đầu để tự cài thư viện Python; sau đó cần Internet cho MQTT/thời tiết.

## 2. Mở đúng project

Trong VS Code chọn **File → Open Folder...** và mở thư mục gốc của project (thư mục có `platformio.ini`, `diagram.json` và `gateway`). Các hướng dẫn bên dưới dùng đường dẫn tương đối, nên không phụ thuộc ổ đĩa hoặc tên tài khoản Windows.

## 3. Build và chạy ESP32 mô phỏng

1. Mở `platformio.ini` hoặc một file `.cpp` trong VS Code để PlatformIO nhận project.
2. Chọn biểu tượng PlatformIO hoặc nhấn `Ctrl+Shift+P`, chạy **PlatformIO: Build**.
3. Chờ Terminal hiện `[SUCCESS]`. Cần build lại sau mỗi lần sửa firmware; dashboard/Python không được build bằng PlatformIO.
4. Mở `diagram.json`, bấm nút xanh **Start Simulation** của Wokwi.
5. Mở Serial Monitor ở tốc độ `115200` để xem log.

Nếu chỉ cần chạy mạch mà chưa dùng dashboard thì dừng ở đây. Build chỉ biên dịch firmware; muốn nhìn mạch hoạt động phải khởi chạy mô phỏng.

## 4. Wi‑Fi và MQTT demo

Firmware hiện được cấu hình sẵn cho Wi‑Fi của Wokwi:

- SSID: `Wokwi-GUEST`
- Mật khẩu: để trống
- MQTT broker demo: `broker.hivemq.com`, cổng `1883` (không mã hóa)

Khi Wokwi đang chạy, ESP32 tự thử kết nối. Serial Monitor báo kết quả Wi‑Fi/MQTT. Thiết bị gửi trạng thái mỗi khoảng 5 giây lên các topic:

| Topic | Dùng để làm gì |
|---|---|
| `mixiplan/tuoicay_mx7k2/trangthai` | ESP32 gửi độ ẩm, mưa, trạng thái bơm |
| `mixiplan/tuoicay_mx7k2/bom/set` | Nhận lệnh `ON`, `OFF`, `AUTO` hoặc lệnh JSON từ dashboard |
| `mixiplan/tuoicay_mx7k2/bom/ack` | ESP32 xác nhận đã nhận/từ chối lệnh |

Broker này là broker công cộng để minh họa; topic có thêm mã riêng `tuoicay_mx7k2` để tránh đụng thiết bị khác. Broker vẫn không ổn định và không bảo mật. Chỉ dùng với Wokwi, không nối bơm thật.

## 5. Chạy dashboard/gateway local

Gateway subscribe MQTT để nhận dữ liệu, lưu lịch sử vào SQLite và cung cấp trang web. Có thể chạy gateway trong khi Wokwi đang mở.

### Khởi động bằng một lệnh trong CMD

Mở **Terminal → New Terminal** trong VS Code. Đảm bảo terminal đang ở thư mục gốc project (dòng lệnh kết thúc bằng tên thư mục project), rồi chạy:

```cmd
gateway\run.bat
```

Lần đầu, script tự tạo `.venv`, cài `requirements.txt` một lần và tạo `gateway/.env` với mật khẩu ngẫu nhiên. Những lần sau chạy lại đúng lệnh trên; script không cài pip lại nếu môi trường đã đủ thư viện. Mở `gateway/.env` để xem mật khẩu đăng nhập. File `.env` được Git bỏ qua.

Nếu vừa thêm thư viện vào `requirements.txt` hoặc cài bị lỗi giữa chừng, chạy lệnh này để cài/cập nhật lại:

```cmd
gateway\run.bat --install
```

Nếu gateway đã chạy trong terminal khác, dừng bản cũ bằng `Ctrl+C` trước. Server mới tiếp tục chạy trong terminal hiện tại. Mở trình duyệt đến:

```text
http://127.0.0.1:8000
```

Đăng nhập bằng `admin` hoặc `viewer`; mở `gateway/.env` để xem mật khẩu đã tạo. Admin được gửi lệnh, viewer chỉ xem. Khi Wokwi và gateway cùng kết nối được broker, dashboard sẽ nhận telemetry.

## 6. Thứ tự chạy đầy đủ

1. Build firmware lại trong VS Code sau thay đổi mới nhất.
2. Khởi chạy Wokwi và để mô phỏng chạy.
3. Mở terminal mới ở thư mục gốc project và chạy `gateway\run.bat`.
4. Mở `http://127.0.0.1:8000` và đăng nhập.
5. Xem MQTT connection, dữ liệu cảm biến, lịch sử và ACK trên dashboard.

Nếu bạn chỉ mở dashboard nhưng Wokwi chưa chạy, dashboard vẫn mở được nhưng chưa có dữ liệu mới. Nếu Wokwi báo license hết hạn, MQTT cũng chưa nhận telemetry từ mô hình.

## 7. Gửi thử lệnh MQTT

Trong dashboard đăng nhập bằng `admin`, bấm **Bật tưới**, **Tắt tưới** hoặc **Tự động**. Lệnh đi từ dashboard → gateway → topic `mixiplan/tuoicay_mx7k2/bom/set` → ESP32; kết quả trả về topic `mixiplan/tuoicay_mx7k2/bom/ack`. Dashboard hiện trạng thái chờ hoặc ACK.

Lệnh cũ dạng text như `ON`, `OFF`, `AUTO` cũng được ESP32 nhận nếu publish trực tiếp tới topic lệnh. Không gửi lệnh nếu đang nối thiết bị tưới thật.

## 8. Webhook (tùy chọn)

Webhook chỉ cần khi một dịch vụ khác POST dữ liệu vào gateway. Mô hình ESP32 hiện gửi MQTT trực tiếp, nên không cần chạy webhook riêng.

## 9. Sự cố thường gặp

| Thông báo | Cách xử lý |
|---|---|
| `'gateway\run.bat' is not recognized` | Terminal không ở thư mục project gốc. Mở đúng folder trong VS Code hoặc chạy `cd /d <đường-dẫn-project>` rồi thử lại. |
| `Python 3 was not found` | Cài Python và bật **Add Python to PATH**, rồi mở terminal VS Code mới. |
| `No module named paho` | Chạy `gateway\run.bat --install` từ thư mục gốc project. |
| `Address already in use` / cổng 8000 bận | Đóng server cũ hoặc chạy với cổng khác, ví dụ `--port 8001`, rồi mở URL tương ứng. |
| Dashboard báo MQTT offline | Kiểm tra Internet, Wokwi có đang chạy, broker có truy cập được và Serial Monitor có báo MQTT connected không. |
| Dashboard không có lịch sử | Build lại firmware, chạy Wokwi, khởi động lại gateway để nạp topic mới; kiểm tra Serial Monitor báo MQTT connected. Gateway và simulator phải cùng broker/topic. SQLite chỉ bắt đầu lưu từ lúc gateway chạy. |
| `Your Wokwi license has expired` | Đây là giới hạn license mô phỏng; build thành công chưa có nghĩa mạch đang chạy. Cần gia hạn/đăng nhập license phù hợp hoặc dùng cách chạy Wokwi hợp lệ của lớp. |
| Gateway không chạy dù đã cài thư viện | Xem lỗi đầu tiên trong terminal; thử `gateway\run.bat --install`, rồi `gateway\run.bat`. |

## 10. Những gì bản demo chưa làm

- MQTT công cộng hiện không TLS/xác thực; topic riêng chỉ giảm đụng tên, không tạo bảo mật. Không dùng với cơ cấu tưới thật.
- Chưa tích hợp LoRa/LoRaWAN, AWS IoT Core, broker riêng hoặc OpenWeatherMap. Mô hình hiện dùng Wi‑Fi/MQTT demo và Open‑Meteo.
- NPK/EC đã bỏ khỏi sơ đồ vì không có đầu dò thật và không tham gia điều khiển tưới.
- Mất điện cần pin/UPS và mạch nguồn; firmware/deep sleep không thể làm thiết bị chạy khi không có điện.
- Khi dùng cảm biến đất thật, cần hiệu chuẩn `SOIL_RAW_DRY`/`SOIL_RAW_WET` và tắt mô phỏng tăng ẩm trong `include/Config.h`.
