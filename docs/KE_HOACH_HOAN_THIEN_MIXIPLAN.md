# Kế hoạch hoàn thiện MixiPlan theo yêu cầu giảng viên

> Tài liệu này mô tả trạng thái code và mô hình sau lượt cập nhật, phân biệt rõ phần đã chạy, phần mô phỏng và phần còn cần triển khai. Có thể dùng làm dàn ý viết báo cáo Word; không trình bày giá trị mô phỏng như kết quả đo từ cảm biến thật.

## 1. Mục tiêu đề tài

MixiPlan là mô hình tưới thông minh dùng độ ẩm đất và dự báo thời tiết để điều khiển bơm/van. ESP32 xử lý dữ liệu tại thiết bị (edge), có thể tiếp tục ra quyết định cục bộ khi Internet hoặc dịch vụ thời tiết không sẵn sàng. Dữ liệu trạng thái được gửi lên máy chủ qua MQTT để phục vụ giám sát.

## 2. So khớp với các phần trong yêu cầu

| Nhóm yêu cầu | Hiện trạng trong project | Việc còn lại |
|---|---|---|
| **2A – Hardware & Embedded**: ESP32, cảm biến, relay/van, lọc và hiệu chuẩn, quyết định tưới | Firmware ESP32; trung bình 10 mẫu ADC; cấu hình mốc khô/ướt; ngưỡng bật 40%, dừng 65%, sinh tồn 15%; tưới xung 2 phút/nghỉ 3 phút; tưới sinh tồn 30 giây; relay bơm và relay van được điều khiển cùng nhau. | Đo/hiệu chuẩn cảm biến thật; xác nhận ngưỡng theo loại cây/đất; kiểm chứng dòng điện và mức logic relay; thử lỗi cảm biến. NPK/EC hiện chỉ là đầu vào potentiometer mô phỏng, chưa phải phép đo hóa học. |
| **2B – Networking & Edge**: LoRa/LoRaWAN, gateway, đệm offline và đồng bộ | ESP32 nối Wi‑Fi, gọi Open‑Meteo, gửi/nhận MQTT; thử kết nối lại; khi forecast quá 15 phút thì chuyển sang điều khiển cục bộ. | Chưa có radio LoRa/LoRaWAN, gateway, hàng đợi telemetry bền vững qua khởi động lại, hay quy trình đồng bộ dữ liệu đã lưu. |
| **2C – Cloud & Security**: AWS IoT Core/OpenWeatherMap, lưu lịch sử, API an toàn, TLS/JWT/RBAC, dashboard | Có gọi Open‑Meteo và xuất JSON MQTT để chứng minh luồng dữ liệu. | Chưa nối AWS IoT Core, cơ sở dữ liệu, backend/API có xác thực, TLS MQTT, JWT/RBAC hay dashboard. HiveMQ public hiện tại chỉ dùng minh họa; không dùng để điều khiển bơm thật. |

**Phạm vi bản demo:** hoàn thiện firmware và kiểm chứng cây quyết định trên Wokwi. Những mục yêu cầu thiết bị thật/dịch vụ cloud được ghi là bước tích hợp, không tuyên bố đã hoàn thành.

## 3. Mô hình Wokwi hiện tại

| Khối trong sơ đồ | Linh kiện mô phỏng | Chân ESP32 | Vai trò |
|---|---|---:|---|
| Bộ điều khiển | ESP32 DevKit | — | Đọc ADC, ra quyết định, Wi‑Fi/MQTT |
| Độ ẩm đất | Potentiometer `DO AM DAT` | GPIO34 | Giá trị ADC giả lập cảm biến điện dung |
| Mưa | Potentiometer `MUA TEST` và nút `TROI MUA` | GPIO35, GPIO27 | Tạo kịch bản mưa để thử thuật toán |
| NPK demo | Potentiometer `NPK DEMO` | GPIO32 | Tín hiệu giả lập, quy đổi minh họa 0–2000 mg/kg |
| EC demo | Potentiometer `EC DEMO` | GPIO33 | Tín hiệu giả lập, quy đổi minh họa 0–5000 µS/cm |
| Bơm | Relay + LED `BOM DEMO` | GPIO26 | LED thể hiện đầu ra điều khiển bơm |
| Van tưới | Relay + LED `VAN 12V DEMO` | GPIO25 | Mô phỏng đầu ra van; không cấp tải 12V thật từ ESP32 |
| Hiển thị | LCD1602 I²C | SDA 21, SCL 22 | Hiện độ ẩm, mưa, trạng thái bơm |

Wokwi hiện không mô phỏng phép đo NPK/EC bằng điện cực hay công nghệ truyền LoRa của project này. Hai potentiometer NPK/EC chỉ tạo đầu vào ADC minh họa cho bài tập đọc/lọc dữ liệu nhiều cảm biến. Cặp relay/LED chỉ minh họa lệnh điều khiển; mạch thật cần nguồn riêng, diode/bảo vệ phù hợp và kiểm tra cách ly điện.

## 4. Luồng hoạt động firmware

1. ESP32 khởi động, giữ relay bơm và van ở trạng thái tắt, sau đó bắt đầu kết nối Wi‑Fi không chờ vô hạn.
2. Mỗi vòng lặp lấy trung bình 10 mẫu ở các kênh ADC. Độ ẩm đất được đổi sang phần trăm từ hai mốc `SOIL_RAW_DRY` và `SOIL_RAW_WET`. Các kênh NPK/EC hiện chỉ quy đổi tuyến tính để hiển thị demo.
3. ESP32 lấy xác suất mưa của giờ hiện tại từ Open‑Meteo. Nếu dữ liệu cũ hơn 15 phút hoặc API không lấy được thì dự báo bị đánh dấu hết hạn; thuật toán không để forecast cũ cản quyết định tưới cục bộ.
4. Nếu đất dưới 15%, bật tưới sinh tồn bất kể dự báo mưa; chu kỳ demo là 30 giây bật, 3 phút nghỉ.
5. Nếu đất dưới 40% và không mưa lớn, bắt đầu tưới theo xung 2 phút bật, 3 phút nghỉ. Mưa lớn từ 70% trở lên thì hoãn chu kỳ thường. Khi đất đạt 65%, dừng tưới.
6. Có lệnh MQTT `ON`, `OFF`, `AUTO` để thử điều khiển. Trạng thái gửi định kỳ dưới dạng JSON lên topic telemetry.
7. Khi mất Wi‑Fi/API, chương trình tiếp tục đọc cảm biến và điều khiển tại ESP32. MQTT/Wi‑Fi tự thử kết nối lại theo khoảng nghỉ.

## 5. Cấu trúc mã nguồn sau khi tách

| File | Trách nhiệm |
|---|---|
| `src/main.cpp` | Khởi tạo và điều phối vòng lặp chính; chọn dữ liệu mưa, cập nhật mô hình đất Wokwi, gọi các module. |
| `include/Config.h` | Chân GPIO, ngưỡng, thời gian tưới, Wi‑Fi, broker và topic demo. |
| `include/AppState.h` | Cấu trúc trạng thái dùng chung giữa các module. |
| `src/Sensors.cpp`, `include/Sensors.h` | Lọc trung bình 10 mẫu ADC, quy đổi độ ẩm, đọc các kênh giả lập NPK/EC/mưa. |
| `src/IrrigationController.cpp`, `include/IrrigationController.h` | Điều kiện AUTO/MANUAL, mưa lớn, tưới sinh tồn và tưới theo xung. |
| `src/WeatherService.cpp`, `include/WeatherService.h` | Gọi Open‑Meteo, đọc xác suất mưa giờ hiện tại, hết hạn forecast. |
| `src/NetworkService.cpp`, `include/NetworkService.h` | Wi‑Fi/MQTT reconnect, nhận lệnh và publish telemetry JSON. |
| `src/Display.cpp`, `include/Display.h` | Khởi tạo và cập nhật LCD/Serial Monitor. |

## 6. Cấu hình và giới hạn cần ghi trong báo cáo

- `SOIL_RAW_DRY = 0` và `SOIL_RAW_WET = 4095` đang phù hợp với potentiometer giả lập. Cảm biến điện dung thật có thể cho chiều ADC ngược; phải đo giá trị khô/ướt và sửa hai hằng này.
- `irrigBoost` là mô hình hóa thay đổi độ ẩm do tưới/mưa để demo vòng phản hồi trong Wokwi. Nó không phải phép đo vật lý. Trước khi chạy cảm biến thật, cần tắt hoặc thay cơ chế này để độ ẩm quyết định chỉ lấy từ cảm biến đã hiệu chuẩn.
- NPK/EC trong JSON có hậu tố `demo`; không diễn giải thành số liệu NPK/EC đo được. Module NPK/EC thật cần mạch giao tiếp, hiệu chuẩn và đơn vị đo cụ thể.
- Xác suất mưa API dùng để minh họa hỗ trợ quyết định; Open‑Meteo không đồng nghĩa OpenWeatherMap. Nếu yêu cầu môn bắt buộc OpenWeatherMap hoặc AWS IoT Core, cần đổi endpoint/tài khoản/cert và kiểm tra license tương ứng.
- Broker `broker.hivemq.com:1883` và topic chung không xác thực, không mã hóa. Chỉ dùng mô phỏng. Hệ thống thật cần broker riêng, TLS, thông tin xác thực và phân quyền.
- Firmware không thể tiếp tục hoạt động khi mất điện hoàn toàn. Muốn chạy khi cúp điện cần pin/UPS và mạch nguồn phù hợp. Khi ESP32 khởi động lại, code mặc định tắt relay trước khi vào AUTO.

## 7. Kịch bản kiểm thử trên Wokwi

1. **Đất đủ ẩm:** đưa `DO AM DAT` lên khoảng 70%; kỳ vọng bơm và van tắt, trạng thái `DU AM`.
2. **Đất khô, không mưa:** đưa độ ẩm xuống khoảng 30%, `MUA TEST` thấp và thả nút mưa; kỳ vọng trạng thái tưới xung. Xung bật dài 2 phút, nghỉ 3 phút.
3. **Đất khô, mưa lớn:** giữ độ ẩm khoảng 30%, chỉnh `MUA TEST` cao trên 70%; kỳ vọng hoãn chu kỳ thường.
4. **Sinh tồn:** đưa độ ẩm dưới 15%, nhấn `TROI MUA`; kỳ vọng vẫn tưới sinh tồn ngắn.
5. **NPK/EC demo:** xoay từng potentiometer; kiểm tra `npk_demo_mgkg`, `ec_demo_us_cm` trên JSON MQTT.
6. **Mất Wi‑Fi/API:** ngắt Internet trong simulator nếu hỗ trợ; xác nhận firmware vẫn quyết định theo đầu vào tại chỗ và forecast hết hạn không còn được áp dụng.
7. **Lệnh tay:** publish `ON`, `OFF`, `AUTO` tới topic điều khiển và quan sát relay/LED.

Với chu kỳ dài, có thể tạm giảm `NORMAL_PULSE_MS`, `SURVIVAL_PULSE_MS` và `PULSE_REST_MS` để demo nhanh, sau đó khôi phục giá trị dùng trong báo cáo.

## 8. Dàn ý chuyển thành báo cáo Word

1. **Kiến trúc:** vẽ các khối cảm biến → ESP32 edge → MQTT/cloud → dashboard; chú giải rõ LoRa/gateway/AWS là phần mở rộng nếu chưa triển khai.
2. **Phần cứng:** bảng linh kiện, chân GPIO, nguồn và sơ đồ mạch Wokwi; phân biệt linh kiện thật với potentiometer/LED thay thế.
3. **Firmware:** giải thích ADC trung bình 10 mẫu, hiệu chuẩn, cây quyết định, khoảng trễ, pulse irrigation, sinh tồn và fallback offline.
4. **Kết nối:** mô tả Wi‑Fi, HTTP Open‑Meteo, MQTT publish/subscribe, JSON và cơ chế reconnect; ghi rõ hạn chế broker demo.
5. **Kiểm thử:** dùng các kịch bản mục 6; chụp ảnh mạch, Serial Monitor, trạng thái relay/LCD và JSON nhận được; điền kết quả thực tế sau khi chạy.
6. **An toàn và giới hạn:** xác thực/TLS, quyền điều khiển, nguồn 12V, mất điện, sai số cảm biến, forecast cũ và các phần chưa có.
7. **Hướng phát triển:** LoRa/LoRaWAN + gateway, buffer bền vững, AWS IoT Core, database, API có JWT/RBAC và dashboard biểu đồ.

## 9. Cách build và mở mô phỏng

1. Mở đúng thư mục `MixiPlan` trong VS Code.
2. Chọn **PlatformIO: Build**; chỉ tiếp tục khi Terminal hiện `[SUCCESS]`.
3. Chạy **Wokwi: Start Simulator** từ `diagram.json`.
4. Mở Serial Monitor ở `115200` baud. Nếu Wokwi hiện license expired, đó là vấn đề license simulator, không phải lỗi firmware.

