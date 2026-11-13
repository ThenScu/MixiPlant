# Kế hoạch hoàn thiện MixiPlan theo yêu cầu giảng viên

> Tài liệu này mô tả trạng thái code và mô hình sau lượt cập nhật, phân biệt rõ phần đã chạy, phần mô phỏng và phần còn cần triển khai. Có thể dùng làm dàn ý viết báo cáo Word; không trình bày giá trị mô phỏng như kết quả đo từ cảm biến thật. Các hình vẽ SVG trong `docs/` chèn trực tiếp vào Word được (Insert → Pictures).

## 1. Mục tiêu đề tài

MixiPlan là mô hình tưới thông minh dùng độ ẩm đất và dự báo thời tiết để điều khiển bơm/van. ESP32 xử lý dữ liệu tại thiết bị (edge), có thể tiếp tục ra quyết định cục bộ khi Internet hoặc dịch vụ thời tiết không sẵn sàng. Dữ liệu trạng thái được gửi lên máy chủ qua MQTT để phục vụ giám sát.

## 2. So khớp với các phần trong yêu cầu

| Nhóm yêu cầu | Hiện trạng trong project | Việc còn lại |
|---|---|---|
| **2A – Hardware & Embedded**: ESP32, cảm biến, relay/van, lọc và hiệu chuẩn, quyết định tưới | Firmware lọc 2 lớp: trimmed mean 10 mẫu ADC + EMA α=0.3; mốc hiệu chuẩn; ngưỡng bật 40%, dừng 65%, sinh tồn 15%; tưới xung 2 phút/nghỉ 3 phút; relay bơm/van; Deep Sleep có điều kiện (mặc định tắt). Có sơ đồ mạch đề xuất với lọc RC + diode flyback (`docs/so_do_mach_loc_va_bao_ve.svg`) và bảng tính pin mục 8. | Chưa đo/hiệu chuẩn cảm biến thật, chưa xác nhận ngưỡng theo cây/đất, dòng relay, thử lỗi cảm biến hay kiểm chứng tiêu thụ điện. Số liệu pin là ước lượng có giả định, chưa đo. NPK/EC không nằm trong demo vì chưa có đầu dò thật. |
| **2B – Networking & Edge**: LoRa/LoRaWAN, gateway, đệm offline và đồng bộ | ESP32 có Wi‑Fi reconnect, Open‑Meteo, MQTT publish/subscribe; **ring buffer 60 gói compact trên ESP32 + tự flush khi MQTT nối lại**; gateway FastAPI nhận MQTT, bung gói backfill, lưu SQLite, webhook có token, ACK lệnh. Đặc tả gói tin gọn ~103 byte (mục 9) vừa 1 frame LoRaWAN SF7. | Chưa có radio LoRa/LoRaWAN thật hay gateway LoRa; buffer nằm trên RAM (mất khi deep sleep/mất điện); webhook chưa nối dịch vụ bên ngoài. |
| **2C – Cloud & Security**: AWS IoT Core/OpenWeatherMap, lưu lịch sử, API an toàn, TLS/JWT/RBAC, dashboard | Dashboard web local đầy đủ: thẻ số liệu, biểu đồ lịch sử, dự báo thời tiết, điều khiển admin, **khối demo đầu vào (ép độ ẩm/mưa)**, nhãn nguồn mô phỏng, ẩn số cũ. SQLite có index thời gian + prune 7 ngày; JWT 8 giờ, RBAC admin/viewer, webhook token, ACK lệnh, cấu hình MQTT TLS tùy chọn. | Chưa deploy AWS IoT Core, chưa chứng chỉ/broker riêng, chưa OpenWeatherMap (đang Open‑Meteo), chưa HTTPS/reverse proxy. Public HiveMQ không an toàn cho thiết bị thật. |

**Phạm vi hiện tại:** firmware, gateway và dashboard đã có mã nguồn trong repo; firmware đã build `[SUCCESS]` (RAM 15.8%, Flash 39.1% với `huge_app.csv`). Việc này không đồng nghĩa đã chạy E2E trên Wokwi hay đã triển khai cloud. Các mục LoRa, AWS, TLS thực tế và đo điện năng vẫn cần phần cứng, tài khoản/chứng thư và kết quả chạy để xác nhận.

## 3. Mô hình Wokwi hiện tại

| Khối trong sơ đồ | Linh kiện mô phỏng | Chân ESP32 | Vai trò |
|---|---|---:|---|
| Bộ điều khiển | ESP32 DevKit | — | Đọc ADC, ra quyết định, Wi‑Fi/MQTT |
| Độ ẩm đất | Núm `1. DO AM DAT` | GPIO34 | Điều chỉnh giá trị độ ẩm |
| Mưa | Núm `2. MUA` và nút `5 NHAN = TROI MUA` | GPIO35, GPIO27 | Tạo tín hiệu mưa để thử thuật toán |
| Bơm | Relay `RELAY BOM` + đèn báo | GPIO26 | Đèn sáng nghĩa là đầu ra bơm đang bật |
| Van tưới | Relay `RELAY VAN` + đèn báo | GPIO25 | Đèn sáng nghĩa là đầu ra van đang mở; van 12V chỉ được mô phỏng |
| Hiển thị chính | LCD1602 I²C, địa chỉ 0x27 | SDA 21, SCL 22 | Hiển thị cố định độ ẩm, mưa, bơm và van |
| Hiển thị thời tiết | LCD1602 I²C, địa chỉ 0x26 | SDA 21, SCL 22 | Nhiệt độ/độ ẩm không khí từ API, forecast mưa và trạng thái MQTT; chung bus I²C |

Demo tập trung vào độ ẩm đất và thời tiết đúng với logic tưới. NPK/EC đã bỏ khỏi sơ đồ vì potentiometer không thể đo dinh dưỡng/độ dẫn điện. Cặp relay/LED chỉ minh họa lệnh điều khiển; mạch thật cần nguồn riêng, diode/bảo vệ phù hợp và kiểm tra cách ly điện (mục 8).

## 4. Luồng hoạt động firmware

1. ESP32 khởi động, giữ relay bơm và van ở trạng thái tắt, sau đó bắt đầu kết nối Wi‑Fi không chờ vô hạn.
2. Mỗi vòng lặp lấy 10 mẫu ADC độ ẩm đất và mưa, **trimmed mean** (bỏ 2 mẫu thấp nhất + 2 cao nhất, trung bình 6 mẫu còn lại) rồi **EMA α=0.3** để chống giật. Độ ẩm đổi sang phần trăm từ hai mốc `SOIL_RAW_DRY` và `SOIL_RAW_WET`.
3. **Override demo từ dashboard**: lệnh `SOIL:x` ép độ ẩm nền, `RAIN:x` ép mưa hiệu lực trong 2 phút. Nếu người dùng vặn núm tương ứng trên mạch lệch quá 5% (`POT_OVERRIDE_CANCEL_DELTA`), override bị hủy ngay — mạch luôn thắng. `SOIL:-1`/`RAIN:-1` hủy sớm.
4. ESP32 lấy xác suất mưa giờ hiện tại từ Open‑Meteo mỗi 10 phút; **khi chưa có dữ liệu hợp lệ, thử lại mỗi 30 giây** thay vì chờ 10 phút. Forecast cũ hơn 15 phút bị đánh dấu hết hạn và không cản quyết định cục bộ.
5. Nếu đất dưới 15%, bật tưới sinh tồn bất kể dự báo mưa; chu kỳ demo 30 giây bật, 3 phút nghỉ.
6. Nếu đất dưới 40% và không mưa lớn, tưới theo xung 2 phút bật, 3 phút nghỉ. Mưa từ 70% trở lên hoãn chu kỳ thường. Đất đạt 65% thì dừng.
7. Lệnh MQTT `ON`, `OFF`, `AUTO`, `SOIL:x`, `RAIN:x` có ACK. Trạng thái gửi định kỳ 5 giây/lần dưới dạng JSON lên topic telemetry.
8. **Khi mất MQTT**: mỗi lần publish 5 giây bị hụt được nén thành gói compact (~103 byte) đưa vào ring buffer 60 ô; khi nối lại, flush toàn bộ theo thứ tự thời gian (mục 9).
9. Khi mất Wi‑Fi/API, chương trình tiếp tục đọc cảm biến và điều khiển tại ESP32; MQTT/Wi‑Fi tự thử lại với backoff 5→60 giây.

## 5. Gateway, webhook và dashboard local

Thư mục `gateway/` chứa API FastAPI và trang web tĩnh. Gateway subscribe telemetry/ACK MQTT (QoS 1) và ghi SQLite; **gói backfill `bf=1` được `expand_backfill()` bung về schema đầy đủ** kèm `backfill=true` và `device_time_ms`. Endpoint `/api/webhooks/telemetry` nhận JSON khi có `X-Webhook-Token`; đăng nhập tạo JWT 8 giờ; viewer chỉ đọc, admin mới được gửi lệnh. Lệnh gắn ID, lưu `queued`, cập nhật `acknowledged/rejected` khi nhận ACK. Lệnh `SOIL:/RAIN:` bị chặn ngoài khoảng −1..100 (HTTP 422). SQLite có index theo `received_at`/`created_at` và **prune dữ liệu cũ hơn 7 ngày** khi khởi động.

Dashboard gồm: 4 thẻ chính (độ ẩm đất, mưa hiệu lực, bơm, MQTT) + 3 thẻ mở rộng (**ADC đất sau lọc, nhiệt độ/độ ẩm không khí**); biểu đồ lịch sử; dự báo hiện tại/8 giờ/3 ngày cho trung tâm TP.HCM (cache 10 phút); khối điều khiển ON/OFF/AUTO; **khối "Demo đầu vào" chỉ admin**: 2 thanh trượt độ ẩm/mưa, nút **Áp dụng** (gửi `SOIL:x` + `RAIN:x`), nút **Trả về mạch** (gửi `SOIL:-1` + `RAIN:-1`), badge "Đất/Mưa đang ép" đọc từ telemetry. Telemetry Wokwi được gắn nhãn mô phỏng; bản tin cũ hơn 20 giây bị ẩn số thay vì trình bày như số đo trực tiếp.

Từ terminal CMD ở thư mục gốc project, chạy `gateway\run.bat`; lần đầu script tự tạo môi trường Python và cài dependencies. Mở `http://127.0.0.1:8000`. Gateway cần Internet để kết nối broker. Broker công cộng chỉ dùng demo; API local không được phơi ra Internet nếu chưa có HTTPS/reverse proxy.

Webhook phù hợp để nhận telemetry từ nền tảng ngoài; firmware hiện publish MQTT trực tiếp, gateway subscribe luồng đó. Webhook chỉ có dữ liệu khi một bên gửi POST với token.

## 6. Cấu trúc mã nguồn sau khi tách

| File | Trách nhiệm |
|---|---|
| `src/main.cpp` | Khởi tạo và điều phối vòng lặp; override demo, chọn mưa hiệu lực, mô hình đất Wokwi, gọi các module. |
| `include/Config.h` | Chân GPIO, ngưỡng, thời gian tưới, Wi‑Fi, broker, topic demo, hằng số override/ring buffer. |
| `include/AppState.h` | Cấu trúc trạng thái dùng chung, gồm trường override và ring buffer. |
| `src/Sensors.cpp` | Trimmed mean 10 mẫu + EMA, quy đổi độ ẩm, đọc mưa và nút báo mưa. |
| `src/IrrigationController.cpp` | AUTO/MANUAL, mưa lớn, tưới sinh tồn, tưới theo xung, trễ chuyển mạch 10 giây. |
| `src/WeatherService.cpp` | Open‑Meteo, xác suất mưa giờ hiện tại, retry 30 giây khi chưa hợp lệ, hết hạn forecast. |
| `src/NetworkService.cpp` | Wi‑Fi/MQTT reconnect, nhận lệnh + ACK, telemetry JSON, gói compact, ring buffer, flush đồng bộ. |
| `src/Display.cpp` | Khởi tạo và cập nhật 2 LCD/Serial Monitor. |
| `src/PowerManager.cpp` | Deep sleep có điều kiện; mặc định tắt, chỉ cho phần cứng thật và AUTO khi đất đủ ẩm. |
| `gateway/app.py`, `gateway/static/index.html` | API, SQLite, JWT/RBAC, MQTT, backfill, webhook, dashboard + khối demo. |

## 7. Câu 1 — Kiến trúc 4 tầng

Hình: `docs/so_do_kien_truc_4_tang.svg` (chèn vào Word).

| Tầng | Thành phần | Thông số/kỹ thuật chính | Giao thức | Dịch vụ cloud |
|---|---|---|---|---|
| 1 · Thiết bị & Edge | ESP32 DevKit v1 | Xtensa LX6 240 MHz, 520 KB SRAM, flash 4 MB, ADC 12-bit, WiFi b/g/n | ADC/GPIO/I²C/UART | — |
| 1 · Thiết bị & Edge | Cảm biến độ ẩm đất (demo: potentiometer) | Analog 0–3.3 V, GPIO34; lọc trimmed mean + EMA | ADC | — |
| 1 · Thiết bị & Edge | Cảm biến mưa | AO GPIO35 + DO GPIO27 (nút nhấn demo) | ADC/GPIO | — |
| 1 · Thiết bị & Edge | Relay bơm + van | 2 relay 5V opto, GPIO26/25; tải 12V nguồn riêng | GPIO | — |
| 1 · Thiết bị & Edge | 2 × LCD1602 I²C | 0x27 + 0x26, SDA21/SCL22 | I²C | — |
| 2 · Kết nối & Gateway | Router WiFi → Internet; MQTT broker | MQTT 3.1.1; demo HiveMQ :1883, mục tiêu AWS IoT Core :8883 TLS | MQTT/TCP, TLS | HiveMQ (demo) |
| 2 · Kết nối & Gateway | Edge Gateway FastAPI (PC local) | Python + uvicorn, subscribe QoS 1, REST, webhook token | HTTP/REST, MQTT | — |
| 3 · Cloud / Lưu trữ | Hiện tại: SQLite WAL + Open‑Meteo | Index thời gian, prune 7 ngày; forecast cache 10 phút | HTTPS | Open‑Meteo |
| 3 · Cloud / Lưu trữ | Mục tiêu đề: AWS IoT Core Rules → Timestream/DynamoDB | Chưa triển khai; cần tài khoản + chứng thư X.509 | MQTT/TLS, HTTPS | AWS, OpenWeatherMap |
| 4 · Ứng dụng | Dashboard web + REST API | HTML/JS thuần, JWT HS256 8h, RBAC admin/viewer, biểu đồ + dự báo | HTTP (loopback) / HTTPS (khi triển khai) | — |

So khớp cloud với yêu cầu đề:

| Hạng mục | Demo hiện tại | Mục tiêu đề tài | Việc cần làm để đạt |
|---|---|---|---|
| Broker MQTT | HiveMQ public, không xác thực | AWS IoT Core | Tạo Thing/policy, endpoint riêng, cert X.509, bật TLS 2 chiều |
| Dự báo thời tiết | Open‑Meteo (miễn phí, không cần key) | OpenWeatherMap nếu đề bắt buộc | Đổi endpoint + API key trong `WeatherService.cpp` và gateway |
| Lưu lịch sử | SQLite trên gateway local | Time-series DB cloud | AWS Timestream/InfluxDB; giữ schema payload JSON |
| Bảo mật kênh | MQTT 1883 plaintext, HTTP loopback | TLS end-to-end + HTTPS | Broker riêng + CA, reverse proxy (Caddy/nginx) cho dashboard |

## 8. 2A — Phần cứng: lọc, hiệu chuẩn, mạch bảo vệ, Deep Sleep và pin

### 8.1. Lọc tín hiệu (đã cài trong firmware)

- **Lớp 1 — trimmed mean**: 10 mẫu ADC cách nhau 2 ms, sắp xếp, bỏ 2 thấp + 2 cao, lấy trung bình 6 mẫu giữa → cắt xung nhiễu tức thời (`readAveragedAdc`, `src/Sensors.cpp`).
- **Lớp 2 — EMA**: `y = y + 0.3·(x − y)` cho cả độ ẩm và mưa → làm mượt biến động chậm, tránh bơm đóng/cắt liên tục.
- **Lớp 3 — trễ chuyển mạch**: `PUMP_MIN_SWITCH_MS = 10 s` trong bộ điều khiển tưới, chống đảo trạng thái relay dồn dập.
- **Phần cứng đề xuất**: lọc RC thông thấp R=1 kΩ + C=100 nF (fc ≈ 1.6 kHz) ngay trước chân ADC; xem `docs/so_do_mach_loc_va_bao_ve.svg`.

### 8.2. Hiệu chuẩn

`SOIL_RAW_DRY = 0`, `SOIL_RAW_WET = 4095` chỉ đúng cho potentiometer Wokwi. Cảm biến điện dung thật thường **ngược chiều** (khô → ADC cao). Quy trình: đo ADC trong không khí khô và trong cốc nước ướt, ghi 2 giá trị vào `Config.h`, kiểm tra lại dải 0–100%. Chưa có số đo thật trong báo cáo này.

### 8.3. Mạch bảo vệ ngõ ra

Hình: `docs/so_do_mach_loc_va_bao_ve.svg`. Relay opto cách ly GPIO với tải 12V; **diode 1N4007 mắc ngược song song cuộn van/bơm** (cathode về +12V) để triệt xung cảm ứng khi ngắt. Nguồn 12V riêng, chung GND với ESP32; thêm cầu chì phía nguồn tải. Wokwi chỉ mô phỏng relay/LED, không kiểm chứng được phần này.

### 8.4. Deep Sleep

`PowerManager.cpp` chỉ ngủ khi đủ điều kiện: `ENABLE_LOW_POWER_DEEP_SLEEP = true`, `SIMULATE_SOIL_RESPONSE = false` (cảm biến thật), chế độ AUTO, bơm đang tắt, không mưa và đất ≥ 65%. Ngủ `LOW_POWER_WAKE_INTERVAL_MS = 300 s` rồi thức bằng timer. **Lưu ý**: ESP32 khởi động lại sau deep sleep (setup chạy lại, relay về tắt, ring buffer RAM mất). Mặc định tắt để Wokwi/MQTT cập nhật liên tục.

### 8.5. Tính toán pin (ước lượng có giả định — chưa đo thực tế)

Giả định: pin 18650 Li-ion 3.7 V – 2600 mAh; mỗi lần thức 8 s (kết nối WiFi + MQTT + đo + publish), dòng trung bình 120 mA (đỉnh TX ~350 mA); ngủ sâu cỡ **module** 10 µA; chu kỳ thức 5 phút. Bơm/van dùng nguồn 12V riêng, không tính vào pin này.

| Đại lượng | Cách tính | Kết quả |
|---|---|---|
| Số lần thức/ngày | 24 h × 60 / 5 phút | 288 lần |
| Thời gian thức/ngày | 288 × 8 s = 2304 s | 0.64 h |
| Dung lượng tiêu thụ khi thức | 0.64 h × 120 mA | 76.8 mAh/ngày |
| Dung lượng khi ngủ (module) | 24 h × 0.01 mA | 0.24 mAh/ngày |
| Tổng (kịch bản module lý tưởng) | 76.8 + 0.24 | ≈ 77 mAh/ngày |
| **Thời gian chạy (module lý tưởng)** | 2600 / 77 | **≈ 33 ngày** |
| Ngủ cấp board DevKit (AMS1117 + CP2102) | giả định 5 mA × 24 h | 120 mAh/ngày |
| **Thời gian chạy (board DevKit thực tế)** | 2600 / (120 + 76.8) | **≈ 13 ngày** |

Kết luận cho báo cáo: deep sleep giúp ích rõ rệt so với chạy liên tục (~120 mA × 24 h ≈ 2880 mAh/ngày, hết pin trong < 1 ngày), nhưng **board DevKit ăn dòng nền đáng kể khi ngủ**; muốn đạt mức tháng cần board ESP32 tối giản (bỏ regulator/USB-UART) hoặc ngắt nguồn ngoại vi. Tất cả con số là ước lượng; Wokwi không mô phỏng tiêu thụ điện, cần đo ampe kế thật trước khi khẳng định.

## 9. 2B — Gói tin tối ưu LoRa, Ring Buffer và đồng bộ lại

### 9.1. Đặc tả gói tin (2B.1)

Hai định dạng cùng tồn tại: **JSON đầy đủ** khi online (dashboard đọc trực tiếp) và **JSON compact** cho ring buffer/đường truyền hẹp như LoRa.

| Trường đầy đủ | Key compact | Ý nghĩa |
|---|---|---|
| — | `bf` | Cờ backfill = 1 |
| — | `t` | `millis()` lúc đóng gói (giờ thiết bị) |
| `am_goc` | `ag` | Độ ẩm nền từ ADC (%) |
| `am_raw` | `ar` | ADC đất sau lọc (0–4095) |
| `am` | `a` | Độ ẩm hiệu lực (%) |
| `mua_api` | `ma` | Xác suất mưa API (%) |
| `mua_pot` | `mp` | Mưa từ núm test (%) |
| `mua` | `m` | Mưa hiệu lực (%) |
| `troi_mua` | `tm` | Nút báo mưa (0/1) |
| `bom` | `b` | Bơm ON/OFF → 1/0 |
| `chedo` | `c` | Chuỗi trạng thái |
| `wifi` | `w` | WiFi connected (0/1) |

Trường bỏ ở gói compact: `du_lieu_mo_phong` (gateway mặc định true), `du_bao_hop_le`, `nhiet_do`, `do_am_kk`, `lenh_id`, `lenh_ketqua`, `mqtt` — không cần cho quyết định tưới, giảm băng thông.

**Kích thước đo được** (giá trị đại diện, JSON không khoảng trắng): compact **103 byte**, đầy đủ **283 byte** (thay đổi theo độ dài chuỗi trạng thái/ID lệnh). Với LoRaWAN băng tần AS923 (Việt Nam), SF7/BW125 chịu tối đa **222 byte/frame**: gói compact vừa 1 frame, gói đầy đủ thì không — đó là lý do tồn tại định dạng compact. Hướng Protobuf (chưa cài, chỉ thiết kế): cùng schema đánh số trường 1–11 ước lượng còn ~30–45 byte, đổi lại phải giữ schema hai đầu và khó đọc bằng mắt khi debug.

### 9.2. Offline Ring Buffer tại Edge (2B.2)

- Sức chứa `TELEMETRY_RING_CAPACITY = 60` ô × chu kỳ 5 s ≈ **5 phút dữ liệu**; đầy thì ghi đè ô cũ nhất (ưu tiên dữ liệu mới).
- Mỗi lần đến hạn publish mà MQTT đứt, `publishTelemetry()` đóng gói compact và `ringPush()` thay vì bỏ dữ liệu.
- Buffer nằm trên RAM: mất khi deep sleep/mất điện. Bản triển khai thật cần ghi NVS/flash hoặc RTC memory — ghi rõ trong phần giới hạn.

### 9.3. Reconnect + tự đồng bộ (2B.3)

Hình: `docs/so_do_dong_bo_offline.svg`.

1. WiFi reconnect mỗi 10 s; MQTT retry backoff 5 s → 60 s.
2. MQTT connect thành công → subscribe lệnh, in `[Sync] Dong bo lai N goi tin da dem`, `flushRing()` gửi lần lượt 60 gói `bf=1` đúng thứ tự cũ→mới.
3. Gateway `on_message` thấy `bf==1` → `expand_backfill()` ánh xạ về schema đầy đủ, thêm `backfill=true`, `device_time_ms`, rồi lưu SQLite như telemetry thường.
4. Dashboard phân biệt điểm backfill qua cờ này; trục thời gian hiện vẽ theo `received_at` nên các điểm dồn về lúc đồng bộ — `device_time_ms` lưu sẵn để nâng cấp trục giờ thiết bị sau.

## 10. 2C — Cloud & Security: lưu trữ time-series, JWT/TLS/RBAC

### 10.1. Thiết kế lưu trữ (time-series)

| Bảng | Cột | Vai trò |
|---|---|---|
| `telemetry` | `id` PK, `received_at` TEXT ISO-8601 UTC, `device_id`, `payload` JSON | Chuỗi thời gian append-only; truy vấn mới nhất/theo thời gian |
| `commands` | `id` PK (uuid), `created_at`, `state`, `status` (queued/acknowledged/rejected) | Vòng đời lệnh điều khiển |
| `command_acks` | `id` PK, `received_at`, `command_id`, `payload` JSON | Bằng chứng thiết bị nhận/từ chối lệnh |

Index: `idx_telemetry_time(id DESC)`, `idx_telemetry_received(received_at DESC)`, `idx_commands_created(created_at DESC)`. SQLite chạy **WAL** cho đọc/ghi đồng thời (MQTT thread + API). `prune_old_data(7)` xóa dữ liệu > 7 ngày khi khởi động để demo không phình DB. Tính time-series: ghi chỉ nối đuôi, đọc theo cửa sổ thời gian, không cập nhật bản ghi cũ — đúng mẫu dữ liệu cảm biến. Quy mô thật nên chuyển InfluxDB/AWS Timestream; schema payload JSON giữ nguyên được.

### 10.2. Xác thực và phân quyền end-to-end

- **Đăng nhập**: POST `/api/login` (admin/viewer, mật khẩu trong `.env`, so sánh `secrets.compare_digest`) → JWT HS256, hạn 8 giờ, claim `sub` + `role`.
- **Mọi API dashboard** yêu cầu `Authorization: Bearer`; token sai/hết hạn → 401.
- **RBAC**: viewer chỉ GET; `POST /api/command` yêu cầu admin (403 nếu viewer). Khối demo đầu vào cũng chỉ admin bấm được.
- **Webhook**: header `X-Webhook-Token` so sánh hằng số thời gian; không có token → 401.
- **Thiết bị**: lệnh MQTT có `id`, ESP32 ACK `accepted/reason`; gateway cập nhật trạng thái lệnh để dashboard theo dõi. Broker demo không xác thực thiết bị — đây là lỗ hổng đã biết, chỉ chấp nhận trong mô phỏng.

### 10.3. TLS và lộ trình bảo mật

| Lớp | Hiện tại | Khi triển khai thật |
|---|---|---|
| ESP32 → broker | plaintext :1883 | `MIXIPLAN_MQTT_TLS 1` + CA/client cert/key trong `include/secrets.h` (từ `secrets.example.h`, không commit) |
| Gateway → broker | plaintext | `MQTT_TLS=1` + `MQTT_CA_CERT`; AWS IoT Core dùng cert X.509 riêng từng Thing |
| Trình duyệt → gateway | HTTP loopback | HTTPS qua reverse proxy (Caddy/nginx), không mở `0.0.0.0` tùy tiện |
| API thời tiết | HTTPS Open‑Meteo | Giữ HTTPS; đổi OpenWeatherMap nếu đề bắt buộc (kèm key) |

Cờ TLS trong code là điều kiện cần, chưa phải bằng chứng kết nối AWS thành công; cần chạy thật với chứng thư để xác nhận.

## 11. Cấu hình và giới hạn cần ghi trong báo cáo

- `SOIL_RAW_DRY = 0` / `SOIL_RAW_WET = 4095` chỉ đúng cho potentiometer; cảm biến thật phải đo lại (mục 8.2).
- `irrigBoost` là mô hình hóa thay đổi độ ẩm do tưới/mưa để demo vòng phản hồi trong Wokwi, không phải phép đo vật lý. Chạy cảm biến thật: đặt `SIMULATE_SOIL_RESPONSE = false`.
- Open‑Meteo không phải OpenWeatherMap; nếu đề bắt buộc thì đổi endpoint/key.
- Broker `broker.hivemq.com:1883` và topic chung không xác thực, không mã hóa — chỉ mô phỏng, không nối bơm thật.
- Ring buffer trên RAM mất khi deep sleep/mất điện; backfill vẽ theo giờ nhận, chưa theo giờ thiết bị.
- JWT/RBAC bảo vệ API ở gateway local; chưa HTTPS thì chỉ dùng loopback. Mật khẩu/secret sinh ngẫu nhiên trong `.env` (gitignore).
- Firmware không thể chạy khi mất điện hoàn toàn; muốn vậy cần pin/UPS (mục 8.5). Khởi động lại luôn tắt relay trước khi vào AUTO.

## 12. Kịch bản kiểm thử trên Wokwi

Các núm/nút: **1** độ ẩm đất, **2** mưa, **3** nhấn báo mưa. LCD chính: độ ẩm/mưa/bơm/van; LCD phụ: nhiệt/ẩm không khí, forecast, MQTT.

1. **Đất đủ ẩm:** `DO AM DAT` ~70% → bơm/van tắt, `DU AM`.
2. **Đất khô, không mưa:** ~30%, `MUA TEST` thấp, thả nút → tưới xung 2 phút bật/3 phút nghỉ.
3. **Đất khô, mưa lớn:** ~30%, `MUA TEST` > 70% → hoãn chu kỳ thường.
4. **Sinh tồn:** < 15% + nhấn `TROI MUA` → vẫn tưới sinh tồn ngắn.
5. **Mất Wi‑Fi/API:** ngắt Internet trong simulator → firmware vẫn quyết định cục bộ, forecast hết hạn không được áp dụng.
6. **Lệnh tay:** publish `ON/OFF/AUTO` tới topic điều khiển → quan sát relay/LED + ACK trên dashboard.
7. **Demo từ dashboard (mới):** đăng nhập admin, kéo thanh độ ẩm về 20%, mưa 0%, bấm **Áp dụng** → Serial in `[Test] Ep do am=20%`, badge "Đất đang ép" hiện, bơm chạy dù núm mạch đang 70%. Vặn núm mạch lệch > 5% → override hủy ngay. Bấm **Trả về mạch** → thiết bị đọc lại từ núm.
8. **Backfill (mới):** để thiết bị chạy, ngắt mạng simulator ~1 phút rồi bật lại → Serial in `[Sync] Dong bo lai N goi tin da dem`; dashboard/API history có các bản ghi `backfill=true`.
9. **Viewer bị chặn (mới):** đăng nhập viewer → nút điều khiển và khối demo bị disabled; gọi thẳng API command bị 403.

Chu kỳ dài có thể tạm giảm `NORMAL_PULSE_MS`, `SURVIVAL_PULSE_MS`, `PULSE_REST_MS` để demo nhanh rồi khôi phục. **Sau mỗi lần sửa code phải Build lại PlatformIO rồi Restart Wokwi** — simulator chỉ đọc `firmware.bin` đã build.

## 13. Dàn ý chuyển thành báo cáo Word

1. **Kiến trúc:** hình `so_do_kien_truc_4_tang.svg` + bảng mục 7; chú giải LoRa/AWS là phần mở rộng chưa triển khai.
2. **Phần cứng:** bảng linh kiện mục 3, hình `so_do_mach_loc_va_bao_ve.svg`, lọc 2 lớp mục 8.1, hiệu chuẩn 8.2, Deep Sleep + bảng pin 8.4–8.5 (ghi rõ giả định).
3. **Firmware:** cây quyết định, khoảng trễ, pulse irrigation, sinh tồn, fallback offline, override demo.
4. **Kết nối:** WiFi/HTTP/MQTT/JSON, bảng đặc tả gói compact mục 9.1, hình `so_do_dong_bo_offline.svg`, reconnect + backfill.
5. **Cloud & Security:** schema DB mục 10.1, luồng JWT/RBAC 10.2, bảng TLS 10.3.
6. **Kiểm thử:** kịch bản mục 12; chụp mạch, Serial, LCD, dashboard, JSON nhận được; điền kết quả thật sau khi chạy, không điền kết quả dự kiến.
7. **An toàn và giới hạn:** mục 11.
8. **Hướng phát triển:** LoRa/LoRaWAN + gateway LoRa, buffer bền vững (NVS/flash), AWS IoT Core, OpenWeatherMap, HTTPS.

## 14. Cách build và mở mô phỏng

1. Mở đúng thư mục `MixiPlan` trong VS Code.
2. **PlatformIO: Build**; chỉ tiếp tục khi Terminal hiện `[SUCCESS]` (lần gần nhất: RAM 15.8%, Flash 39.1%).
3. **Wokwi: Start Simulator** từ `diagram.json`; nếu đang chạy bản cũ thì Restart để nạp firmware mới.
4. Serial Monitor `115200`. Wokwi báo license expired là vấn đề license simulator, không phải lỗi firmware.
5. Gateway: `gateway\run.bat` từ CMD ở thư mục gốc, mở `http://127.0.0.1:8000`, mật khẩu trong `gateway/.env`.

## 15. Các phần chưa thể hoàn tất chỉ bằng code

1. **LoRa/LoRaWAN:** cần module radio, gateway, cấu hình vùng tần số (AS923) và thiết bị đầu cuối; gói compact đã sẵn sàng về mặt kích thước nhưng chưa có đường truyền LoRa thật.
2. **Mất điện:** cần pin/UPS, mạch sạc/chuyển nguồn, bảo vệ bơm; deep sleep chỉ giảm điện khi còn nguồn. Buffer RAM không sống sót qua mất điện.
3. **AWS IoT Core:** cần tài khoản, Thing/policy, endpoint, CA/chứng thư/key; khóa riêng để cục bộ, không commit.
4. **NPK/EC:** chỉ thêm nếu GV yêu cầu riêng và có đầu dò thật + mạch chuyển đổi/hiệu chuẩn.
5. **Bằng chứng báo cáo:** chạy kịch bản mục 12, lưu ảnh và số liệu thật, thảo luận sai số; đo dòng điện thật cho bảng pin.