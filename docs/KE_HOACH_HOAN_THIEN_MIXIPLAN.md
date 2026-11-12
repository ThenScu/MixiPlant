# Kế hoạch hoàn thiện MixiPlan theo yêu cầu giảng viên

> Tài liệu này mô tả trạng thái code và mô hình sau lượt cập nhật, phân biệt rõ phần đã chạy, phần mô phỏng và phần còn cần triển khai. Có thể dùng làm dàn ý viết báo cáo Word; không trình bày giá trị mô phỏng như kết quả đo từ cảm biến thật.

## 1. Mục tiêu đề tài

MixiPlan là mô hình tưới thông minh dùng độ ẩm đất và dự báo thời tiết để điều khiển bơm/van. ESP32 xử lý dữ liệu tại thiết bị (edge), có thể tiếp tục ra quyết định cục bộ khi Internet hoặc dịch vụ thời tiết không sẵn sàng. Dữ liệu trạng thái được gửi lên máy chủ qua MQTT để phục vụ giám sát.

## 2. So khớp với các phần trong yêu cầu

| Nhóm yêu cầu | Hiện trạng trong project | Việc còn lại |
|---|---|---|
| **2A – Hardware & Embedded**: ESP32, cảm biến, relay/van, lọc và hiệu chuẩn, quyết định tưới | Firmware có lọc trung bình 10 mẫu ADC độ ẩm/mưa; mốc hiệu chuẩn; ngưỡng bật 40%, dừng 65%, sinh tồn 15%; tưới xung 2 phút/nghỉ 3 phút; điều khiển relay bơm/van. Có cờ mô phỏng độ ẩm Wokwi và tùy chọn deep sleep cho phần cứng. | Chưa đo/hiệu chuẩn cảm biến thật, chưa xác nhận ngưỡng theo cây/đất, dòng relay, thử lỗi cảm biến hay kiểm chứng tiêu thụ điện. NPK/EC không nằm trong demo vì chưa có đầu dò thật và không dùng cho quyết định tưới. |
| **2B – Networking & Edge**: LoRa/LoRaWAN, gateway, đệm offline và đồng bộ | ESP32 có Wi‑Fi reconnect, Open‑Meteo, MQTT publish/subscribe; gateway FastAPI mới nhận MQTT, lưu SQLite, nhận webhook có token và theo dõi ACK lệnh. ESP32 vẫn điều khiển cục bộ khi mạng/API mất. | Chưa có radio LoRa/LoRaWAN hoặc gateway LoRa; chưa có buffer telemetry bền vững phía ESP32 khi mất mạng; webhook mới là API nhận dữ liệu cục bộ, chưa được nối vào dịch vụ bên ngoài. |
| **2C – Cloud & Security**: AWS IoT Core/OpenWeatherMap, lưu lịch sử, API an toàn, TLS/JWT/RBAC, dashboard | Có dashboard web local, SQLite, API lịch sử, JWT đăng nhập admin/viewer, giới hạn điều khiển cho admin, webhook token, ACK lệnh, cấu hình MQTT TLS/cert tùy chọn và file mẫu secrets. | Chưa deploy lên AWS IoT Core, chưa cấu hình chứng chỉ/broker riêng, chưa dùng OpenWeatherMap (đang dùng Open‑Meteo), chưa có HTTPS/reverse proxy khi truy cập từ Internet. Public HiveMQ không an toàn cho thiết bị thật. |

**Phạm vi hiện tại:** firmware, gateway và dashboard đã có mã nguồn trong repo; việc này không đồng nghĩa đã build/run thành công hay đã triển khai cloud. Các mục LoRa, AWS, TLS thực tế và đo điện năng vẫn cần phần cứng, tài khoản/chứng thư và kết quả chạy để xác nhận.

## 3. Mô hình Wokwi hiện tại

| Khối trong sơ đồ | Linh kiện mô phỏng | Chân ESP32 | Vai trò |
|---|---|---:|---|
| Bộ điều khiển | ESP32 DevKit | — | Đọc ADC, ra quyết định, Wi‑Fi/MQTT |
| Độ ẩm đất | Núm `1. DO AM DAT` | GPIO34 | Điều chỉnh giá trị độ ẩm |
| Mưa | Núm `2. MUA` và nút `5 NHAN = TROI MUA` | GPIO35, GPIO27 | Tạo tín hiệu mưa để thử thuật toán |
| Bơm | Relay `RELAY BOM` + đèn báo | GPIO26 | Đèn sáng nghĩa là đầu ra bơm đang bật |
| Van tưới | Relay `RELAY VAN` + đèn báo | GPIO25 | Đèn sáng nghĩa là đầu ra van đang mở; van 12V chỉ được mô phỏng |
| Hiển thị chính | LCD1602 I²C, địa chỉ 0x27 | SDA 21, SCL 22 | Hiển thị cố định độ ẩm, mưa, bơm và van |
| Hiển thị thời tiết | LCD1602 I²C, địa chỉ 0x26 | SDA 21, SCL 22 | Hiển thị nhiệt độ/độ ẩm không khí từ API, forecast mưa và trạng thái MQTT; dùng chung bus I²C với màn hình chính |

Demo hiện tập trung vào độ ẩm đất và thời tiết đúng với logic tưới. NPK/EC đã bỏ khỏi sơ đồ vì potentiometer không thể đo dinh dưỡng/độ dẫn điện và dữ liệu giả làm dashboard khó hiểu. Cặp relay/LED chỉ minh họa lệnh điều khiển; mạch thật cần nguồn riêng, diode/bảo vệ phù hợp và kiểm tra cách ly điện.

## 4. Luồng hoạt động firmware

1. ESP32 khởi động, giữ relay bơm và van ở trạng thái tắt, sau đó bắt đầu kết nối Wi‑Fi không chờ vô hạn.
2. Mỗi vòng lặp lấy trung bình 10 mẫu ở kênh ADC độ ẩm đất và mưa. Độ ẩm được đổi sang phần trăm từ hai mốc `SOIL_RAW_DRY` và `SOIL_RAW_WET`.
3. ESP32 lấy xác suất mưa của giờ hiện tại từ Open‑Meteo. Nếu dữ liệu cũ hơn 15 phút hoặc API không lấy được thì dự báo bị đánh dấu hết hạn; thuật toán không để forecast cũ cản quyết định tưới cục bộ.
4. Nếu đất dưới 15%, bật tưới sinh tồn bất kể dự báo mưa; chu kỳ demo là 30 giây bật, 3 phút nghỉ.
5. Nếu đất dưới 40% và không mưa lớn, bắt đầu tưới theo xung 2 phút bật, 3 phút nghỉ. Mưa lớn từ 70% trở lên thì hoãn chu kỳ thường. Khi đất đạt 65%, dừng tưới.
6. Có lệnh MQTT `ON`, `OFF`, `AUTO` để thử điều khiển. Trạng thái gửi định kỳ dưới dạng JSON lên topic telemetry.
7. Khi mất Wi‑Fi/API, chương trình tiếp tục đọc cảm biến và điều khiển tại ESP32. MQTT/Wi‑Fi tự thử kết nối lại theo khoảng nghỉ.

## 5. Gateway, webhook và dashboard local

Thư mục `gateway/` chứa API FastAPI và trang web tĩnh. Gateway subscribe telemetry/ACK MQTT và ghi SQLite; endpoint `/api/webhooks/telemetry` nhận JSON khi có `X-Webhook-Token`; đăng nhập tạo JWT 8 giờ; viewer chỉ đọc, admin mới được gửi lệnh `ON/OFF/AUTO`. Lệnh được gắn ID, gateway lưu trạng thái queued và cập nhật khi nhận ACK từ ESP32. Dashboard vẽ lịch sử độ ẩm/mưa, hiển thị trạng thái MQTT/ACK và lấy thời tiết hiện tại, dự báo 8 giờ/3 ngày cho trung tâm TP.HCM qua Open‑Meteo; API thời tiết cache 10 phút. Telemetry đánh dấu Wokwi là mô phỏng; khi không có bản tin mới quá 20 giây, dashboard ẩn các số cũ thay vì trình bày như số đo trực tiếp.

Từ terminal CMD ở thư mục gốc project, chạy `gateway\run.bat`; lần đầu script tự tạo môi trường Python và cài dependencies, các lần sau không cài lại. Mở `http://127.0.0.1:8000`. Gateway cần Internet để kết nối broker. Broker công cộng chỉ dùng demo; API local không được phơi ra Internet nếu chưa có HTTPS/reverse proxy.

Webhook phù hợp để nhận telemetry từ nền tảng ngoài hoặc tích hợp sau này; firmware hiện publish MQTT trực tiếp, gateway subscribe luồng đó. Hai cách không tự động đồng nhất: webhook sẽ chỉ có dữ liệu nếu một bên gửi POST với token.

## 6. Cấu trúc mã nguồn sau khi tách

| File | Trách nhiệm |
|---|---|
| `src/main.cpp` | Khởi tạo và điều phối vòng lặp chính; chọn dữ liệu mưa, cập nhật mô hình đất Wokwi, gọi các module. |
| `include/Config.h` | Chân GPIO, ngưỡng, thời gian tưới, Wi‑Fi, broker và topic demo. |
| `include/AppState.h` | Cấu trúc trạng thái dùng chung giữa các module. |
| `src/Sensors.cpp`, `include/Sensors.h` | Lọc trung bình 10 mẫu ADC, quy đổi độ ẩm, đọc mưa và nút báo mưa. |
| `src/IrrigationController.cpp`, `include/IrrigationController.h` | Điều kiện AUTO/MANUAL, mưa lớn, tưới sinh tồn và tưới theo xung. |
| `src/WeatherService.cpp`, `include/WeatherService.h` | Gọi Open‑Meteo, đọc xác suất mưa giờ hiện tại, hết hạn forecast. |
| `src/NetworkService.cpp`, `include/NetworkService.h` | Wi‑Fi/MQTT reconnect, nhận lệnh và publish telemetry JSON. |
| `src/Display.cpp`, `include/Display.h` | Khởi tạo và cập nhật LCD/Serial Monitor. |
| `src/PowerManager.cpp`, `include/PowerManager.h` | Deep sleep tùy chọn; mặc định tắt, chỉ dành cho cảm biến thật và AUTO khi đất đủ ẩm. |
| `gateway/app.py`, `gateway/static/index.html` | API, SQLite, JWT/RBAC, MQTT, webhook và dashboard local. |

## 7. Cấu hình và giới hạn cần ghi trong báo cáo

- `SOIL_RAW_DRY = 0` và `SOIL_RAW_WET = 4095` đang phù hợp với potentiometer giả lập. Cảm biến điện dung thật có thể cho chiều ADC ngược; phải đo giá trị khô/ướt và sửa hai hằng này.
- `irrigBoost` là mô hình hóa thay đổi độ ẩm do tưới/mưa để demo vòng phản hồi trong Wokwi. Nó không phải phép đo vật lý. Trước khi chạy cảm biến thật, cần tắt hoặc thay cơ chế này để độ ẩm quyết định chỉ lấy từ cảm biến đã hiệu chuẩn.
- Xác suất mưa API dùng để minh họa hỗ trợ quyết định; Open‑Meteo không đồng nghĩa OpenWeatherMap. Nếu yêu cầu môn bắt buộc OpenWeatherMap hoặc AWS IoT Core, cần đổi endpoint/tài khoản/cert và kiểm tra license tương ứng.
- Broker `broker.hivemq.com:1883` và topic chung không xác thực, không mã hóa. Chỉ dùng mô phỏng. Hệ thống thật cần broker riêng, TLS, thông tin xác thực và phân quyền.
- Bật MQTT TLS bằng `MIXIPLAN_MQTT_TLS 1` và cấu hình CA/chứng thư trong `include/secrets.h` (tạo từ `include/secrets.example.h`, không commit secrets). Cờ TLS chưa phải bằng chứng kết nối AWS thành công. Gateway cũng cần `MQTT_TLS=1` và CA tương ứng.
- JWT/RBAC hiện bảo vệ API dashboard ở gateway local; vì chưa có HTTPS, chỉ dùng loopback trong mạng phát triển. Tạo mật khẩu/secret mạnh từ `.env.example` trước khi khởi động.
- Firmware không thể tiếp tục hoạt động khi mất điện hoàn toàn. Muốn chạy khi cúp điện cần pin/UPS và mạch nguồn phù hợp. Khi ESP32 khởi động lại, code mặc định tắt relay trước khi vào AUTO.

## 8. Kịch bản kiểm thử trên Wokwi

Các núm và nút trên mạch gồm **1** độ ẩm đất, **2** mưa, **3** nhấn để báo trời mưa. LCD chính hiển thị độ ẩm/mưa hiệu lực và trạng thái bơm/van; LCD thứ hai hiển thị nhiệt độ/độ ẩm không khí, forecast mưa và MQTT. Hai LCD dùng chung SDA/SCL nhưng có địa chỉ riêng (0x27 và 0x26). Hai relay là công tắc do ESP32 điều khiển; đèn tương ứng sáng khi đầu ra bật.

1. **Đất đủ ẩm:** đưa `DO AM DAT` lên khoảng 70%; kỳ vọng bơm và van tắt, trạng thái `DU AM`.
2. **Đất khô, không mưa:** đưa độ ẩm xuống khoảng 30%, `MUA TEST` thấp và thả nút mưa; kỳ vọng trạng thái tưới xung. Xung bật dài 2 phút, nghỉ 3 phút.
3. **Đất khô, mưa lớn:** giữ độ ẩm khoảng 30%, chỉnh `MUA TEST` cao trên 70%; kỳ vọng hoãn chu kỳ thường.
4. **Sinh tồn:** đưa độ ẩm dưới 15%, nhấn `TROI MUA`; kỳ vọng vẫn tưới sinh tồn ngắn.
5. **Mất Wi‑Fi/API:** ngắt Internet trong simulator nếu hỗ trợ; xác nhận firmware vẫn quyết định theo đầu vào tại chỗ và forecast hết hạn không còn được áp dụng.
6. **Lệnh tay:** publish `ON`, `OFF`, `AUTO` tới topic điều khiển và quan sát relay/LED.

Với chu kỳ dài, có thể tạm giảm `NORMAL_PULSE_MS`, `SURVIVAL_PULSE_MS` và `PULSE_REST_MS` để demo nhanh, sau đó khôi phục giá trị dùng trong báo cáo.

## 9. Dàn ý chuyển thành báo cáo Word

1. **Kiến trúc:** vẽ các khối cảm biến → ESP32 edge → MQTT/cloud → dashboard; chú giải rõ LoRa/gateway/AWS là phần mở rộng nếu chưa triển khai.
2. **Phần cứng:** bảng linh kiện, chân GPIO, nguồn và sơ đồ mạch Wokwi; phân biệt linh kiện thật với potentiometer/LED thay thế.
3. **Firmware:** giải thích ADC trung bình 10 mẫu, hiệu chuẩn, cây quyết định, khoảng trễ, pulse irrigation, sinh tồn và fallback offline.
4. **Kết nối:** mô tả Wi‑Fi, HTTP Open‑Meteo, MQTT publish/subscribe, JSON và cơ chế reconnect; ghi rõ hạn chế broker demo.
5. **Kiểm thử:** dùng các kịch bản mục 8; chụp ảnh mạch, Serial Monitor, trạng thái relay/LCD và JSON nhận được; điền kết quả thực tế sau khi chạy.
6. **An toàn và giới hạn:** xác thực/TLS, quyền điều khiển, nguồn 12V, mất điện, sai số cảm biến, forecast cũ và các phần chưa có.
7. **Hướng phát triển:** LoRa/LoRaWAN + gateway LoRa, buffer bền vững phía ESP32, AWS IoT Core và OpenWeatherMap nếu bắt buộc theo đề; thêm HTTPS khi triển khai công khai.

## 10. Cách build và mở mô phỏng

1. Mở đúng thư mục `MixiPlan` trong VS Code.
2. Chọn **PlatformIO: Build**; chỉ tiếp tục khi Terminal hiện `[SUCCESS]`.
3. Chạy **Wokwi: Start Simulator** từ `diagram.json`.
4. Mở Serial Monitor ở `115200` baud. Nếu Wokwi hiện license expired, đó là vấn đề license simulator, không phải lỗi firmware.

## 11. Các phần chưa thể hoàn tất chỉ bằng code

1. **LoRa/LoRaWAN:** cần module radio tương thích, gateway (hoặc gateway dịch vụ), cấu hình vùng tần số và thiết bị đầu cuối; sơ đồ hiện chưa có module LoRa.
2. **Mất điện:** firmware không thể cấp điện cho ESP32/bơm. Cần pin/UPS, mạch sạc/chuyển nguồn, bảo vệ bơm và thử thời gian lưu điện. Deep sleep chỉ giảm điện năng khi thiết bị vẫn được cấp nguồn.
3. **AWS IoT Core:** cần tài khoản AWS, Thing/policy, endpoint, CA/chứng thư/key; đưa khóa riêng vào máy cục bộ, không commit.
4. **NPK/EC:** chỉ bổ sung nếu GV yêu cầu riêng và nhóm có đầu dò thật cùng mạch chuyển đổi/hiệu chuẩn; không đưa số liệu potentiometer thành kết quả đo.
5. **Bằng chứng báo cáo:** chạy các kịch bản, lưu ảnh Serial/LCD/relay/dashboard, ghi kết quả thật và thảo luận sai số. Không tự điền kết quả dự kiến thành kết quả đã đo.
