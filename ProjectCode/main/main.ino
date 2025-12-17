#include <Arduino_GFX_Library.h>
#include <SD.h>
#include <SPI.h>
#include <TJpg_Decoder.h>

// ===== 颜色定义 (RGB565) =====
#define BLACK   0x0000
#define WHITE   0xFFFF
#define RED     0xF800
#define YELLOW  0xFFE0

// ===== TFT 配置 =====
Arduino_ESP32SPI *bus = new Arduino_ESP32SPI(
  41,  // DC
  42,  // CS
  21,  // SCK
  47,  // MOSI
  -1   // MISO (不需要)
);

Arduino_ST7789 *gfx = new Arduino_ST7789(
  bus,
  -1,    // RST
  0,     // rotation
  false, // IPS
  240,   // 宽度
  320    // 高度
);

// ===== SD 卡配置 =====
SPIClass sdSPI(HSPI);

// ===== 变量定义 =====
#define MAX_FILES 10
String jpgFiles[MAX_FILES];
uint8_t fileCount = 0;
String folder = "/PIC";

// ===== JPEG 解码回调函数 =====
bool tft_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap) {
  if (y >= gfx->height()) return 0;
  gfx->draw16bitRGBBitmap(x, y, bitmap, w, h);
  return 1;
}

// ===== 判断是否为 JPG 文件 =====
bool isJpgFile(String name) {
  return !name.startsWith(".") && 
         (name.endsWith(".jpg") || name.endsWith(".JPG") || 
          name.endsWith(".jpeg") || name.endsWith(".JPEG"));
}

// ===== 列出文件夹中的 JPEG 文件 =====
void listJpgFiles(String dir) {
  File root = SD.open(dir);
  if (!root) {
    Serial.println("❌ 打开文件夹失败！");
    gfx->setCursor(10, 100);
    gfx->setTextColor(RED);
    gfx->println("Folder not found!");
    return;
  }
  
  fileCount = 0;
  while (true) {
    File file = root.openNextFile();
    if (!file) break;
    
    if (!file.isDirectory() && isJpgFile(file.name())) {
      if (fileCount < MAX_FILES) {
        jpgFiles[fileCount] = file.name();
        fileCount++;
      } else {
        break;
      }
    }
    file.close();
  }
  root.close();
  
  Serial.print("📊 共找到 ");
  Serial.print(fileCount);
  Serial.println(" 个图片文件");
}

// ==========================================
// ===== [修改重点] 缩放并居中显示图片 =====
// ==========================================
void scalePic(String name) {
  uint16_t w = 0, h = 0;
  // 获取图片的原始尺寸
  TJpgDec.getSdJpgSize(&w, &h, name);
  
  uint16_t screenW = gfx->width();
  uint16_t screenH = gfx->height();

  // 1. 计算合适的缩放因子 (1, 2, 4, 8)
  // 必须同时满足 宽<=屏幕宽 且 高<=屏幕高
  int scale = 1;
  if (w > screenW || h > screenH) {
    if ((w / 2) <= screenW && (h / 2) <= screenH) {
      scale = 2;
    } else if ((w / 4) <= screenW && (h / 4) <= screenH) {
      scale = 4;
    } else {
      scale = 8; // 最大只能缩放 1/8
    }
  }
  
  TJpgDec.setJpgScale(scale);

  // 2. 计算居中坐标
  // 缩放后的实际图片宽高
  uint16_t scaled_w = w / scale;
  uint16_t scaled_h = h / scale;
  
  // 居中公式：(屏幕尺寸 - 图片尺寸) / 2
  int16_t x = (screenW - scaled_w) / 2;
  int16_t y = (screenH - scaled_h) / 2;

  // 3. 打印调试信息
  Serial.printf("   尺寸: %dx%d -> 缩放: 1/%d -> 位置: (%d, %d)\n", w, h, scale, x, y);
  
  // 4. 在计算出的坐标处绘制
  TJpgDec.drawSdJpg(x, y, name);
}

// ===== Setup =====
void setup() {
  Serial.begin(115200);
  delay(1000);
  
  // 初始化 TFT
  gfx->begin();
  gfx->fillScreen(BLACK);
  gfx->setTextSize(2);
  gfx->setTextColor(WHITE);
  
  // 初始化 SD 卡
  sdSPI.begin(39, 40, 38, 1); // SCK, MISO, MOSI, CS
  if (!SD.begin(1, sdSPI, 80000000)) {
    gfx->fillScreen(RED);
    gfx->setCursor(10, 100);
    gfx->println("SD Card Failed!");
    while (1) delay(1000);
  }
  
  // 配置解码器
  TJpgDec.setCallback(tft_output);
  
  // 查找文件
  listJpgFiles(folder);
  
  if (fileCount == 0) {
    gfx->fillScreen(YELLOW);
    gfx->setTextColor(BLACK);
    gfx->setCursor(10, 100);
    gfx->println("No images found!");
    while (1) delay(1000);
  }
  
  // 首次播放
  for (int i = 0; i < fileCount; i++) {
    String fullPath = (folder == "/") ? ("/" + jpgFiles[i]) : (folder + "/" + jpgFiles[i]);
    Serial.println("显示: " + fullPath);
    
    // 清屏放在绘制之前，或者绘制后等待再清屏
    // 为了防止居中时边缘有上一张图的残留，必须先清屏
    gfx->fillScreen(BLACK); 
    scalePic(fullPath);
    delay(2000);
  }
}

// ===== Loop =====
void loop() {
  for (int i = 0; i < fileCount; i++) {
    String fullPath = (folder == "/") ? ("/" + jpgFiles[i]) : (folder + "/" + jpgFiles[i]);
    
    // 每次显示前清屏（黑色背景），确保居中时周围是黑色的
    gfx->fillScreen(BLACK);
    
    scalePic(fullPath);
    delay(2000);
  }
}