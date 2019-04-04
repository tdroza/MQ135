#include <FS.h>

void readConfig()
{
  //read config.json and load configuration to variables.
  File configFile = SPIFFS.open("/config.jsn", "r");
  if (!configFile)
  {
    Serial.println("Failed to open config file");
    while (1)
      ;
  }
  size_t size = configFile.size();
  
  Serial.print("Config File Size: ");
  Serial.println(String (size));
  
  if (size > 1024)
  {
    Serial.println("Config file size is too large");
    while (1)
      ;
  }
  std::unique_ptr<char[]> buf(new char[size]);
  configFile.readBytes(buf.get(), size);
  StaticJsonBuffer<700> jsonBuffer;
  JsonObject &json = jsonBuffer.parseObject(buf.get());
  if (!json.success())
  {
    Serial.println("Failed to parse config file");
    while (1)
      ;
  }

  ssid = (const char *)json["ssid"];
  password = (const char *)json["pass"];
  host = (const char *)json["host"];
  url = (const char *)json["uri"];
  wc_p = json["wc_p"];
  gr_p = json["gr_p"];
  reporting_interval_mins = json["reporting_interval_mins"];
  s_vcc = json["s_vcc"];
  is_ip = json["is_ip"];
  vcc_parm = (const char *)json["vcc_p"];
  s_lowbattery = json["s_lowbattery"];
  lowbattery_threshold = json["lowbattery_threshold"];
  lowbattery_uri = (const char *)json["lowbattery_uri"];

  Serial.println("Parsed JSON Config.");
  Serial.print("Loaded ssid: ");
  Serial.println(ssid);
  Serial.print("Loaded password: ");
  Serial.println(password);
  Serial.print("Loaded host: ");
  Serial.println(host);
  Serial.print("Loaded IsIP: ");
  Serial.println(is_ip);
  Serial.print("Loaded uri: ");
  Serial.println(url);
  Serial.print("Loaded WiFi Connect Persistance: ");
  Serial.println(wc_p);
  Serial.print("Loaded GET Request Persistance: ");
  Serial.println(gr_p);
  Serial.print("Loaded Reporting Interval (mins): ");
  Serial.println(reporting_interval_mins);
  Serial.print("Loaded Send VCC: ");
  Serial.println(s_vcc);
  Serial.print("Loaded VCC Param.: ");
  Serial.println(vcc_parm);
  Serial.println(s_lowbattery);
  Serial.print("Loaded Low Batttery Threshold: ");
  Serial.println(lowbattery_threshold);
  Serial.print("Loaded Low Batttery URI: ");
  Serial.println(lowbattery_uri);
  Serial.println();
}

/*
   Config. Mode Functions
*/

//edit functions
String formatBytes(size_t bytes)
{
  if (bytes < 1024)
  {
    return String(bytes) + "B";
  }
  else if (bytes < (1024 * 1024))
  {
    return String(bytes / 1024.0) + "KB";
  }
  else if (bytes < (1024 * 1024 * 1024))
  {
    return String(bytes / 1024.0 / 1024.0) + "MB";
  }
  else
  {
    return String(bytes / 1024.0 / 1024.0 / 1024.0) + "GB";
  }
}

String getContentType(String filename)
{
  if (server.hasArg("download"))
    return "application/octet-stream";
  else if (filename.endsWith(".htm"))
    return "text/html";
  else if (filename.endsWith(".html"))
    return "text/html";
  else if (filename.endsWith(".css"))
    return "text/css";
  else if (filename.endsWith(".js"))
    return "application/javascript";
  else if (filename.endsWith(".png"))
    return "image/png";
  else if (filename.endsWith(".gif"))
    return "image/gif";
  else if (filename.endsWith(".jpg"))
    return "image/jpeg";
  else if (filename.endsWith(".ico"))
    return "image/x-icon";
  else if (filename.endsWith(".xml"))
    return "text/xml";
  else if (filename.endsWith(".pdf"))
    return "application/x-pdf";
  else if (filename.endsWith(".zip"))
    return "application/x-zip";
  else if (filename.endsWith(".gz"))
    return "application/x-gzip";
  return "text/plain";
}

bool handleFileRead(String path)
{
  Serial.println("handleFileRead: " + path);
  if ((path == "/") && (server.argName(0) == "restart" && server.arg(0) == "true"))
  {
    Serial.println(F("requested reset from admin page!"));
    server.send(200, "text/plain", "Restarting!");
#ifdef NOT_DEBUG
    digitalWrite(LED_BUILTIN, HIGH);
#endif
    delay(200);
    wdt_reset();
    ESP.restart();
    while (1)
    {
      wdt_reset();
    }
    //WiFi.forceSleepBegin(); wdt_reset(); ESP.restart(); while (1)wdt_reset();
  }
  if (path.endsWith("/"))
    path += "index.htm";
  String contentType = getContentType(path);
  String pathWithGz = path + ".gz";
  if (SPIFFS.exists(pathWithGz) || SPIFFS.exists(path))
  {
    if (SPIFFS.exists(pathWithGz))
      path += ".gz";
    File file = SPIFFS.open(path, "r");
    size_t sent = server.streamFile(file, contentType);
    file.close();
    return true;
  }
  return false;
}

void handleFileUpload()
{
  if (server.uri() != "/edit")
    return;
  HTTPUpload &upload = server.upload();
  if (upload.status == UPLOAD_FILE_START)
  {
    String filename = upload.filename;
    if (!filename.startsWith("/"))
      filename = "/" + filename;
    Serial.print("handleFileUpload Name: ");
    Serial.println(filename);
    fsUploadFile = SPIFFS.open(filename, "w");
    filename = String();
  }
  else if (upload.status == UPLOAD_FILE_WRITE)
  {
    //Serial.print("handleFileUpload Data: "); Serial.println(upload.currentSize);
    if (fsUploadFile)
      fsUploadFile.write(upload.buf, upload.currentSize);
  }
  else if (upload.status == UPLOAD_FILE_END)
  {
    if (fsUploadFile)
      fsUploadFile.close();
    Serial.print("handleFileUpload Size: ");
    Serial.println(upload.totalSize);
  }
}

void handleFileDelete()
{
  if (server.args() == 0)
    return server.send(500, "text/plain", "BAD ARGS");
  String path = server.arg(0);
  Serial.println("handleFileDelete: " + path);
  if (path == "/")
    return server.send(500, "text/plain", "BAD PATH");
  if (!SPIFFS.exists(path))
    return server.send(404, "text/plain", "FileNotFound");
  SPIFFS.remove(path);
  server.send(200, "text/plain", "");
  path = String();
}

void handleFileCreate()
{
  if (server.args() == 0)
    return server.send(500, "text/plain", "BAD ARGS");
  String path = server.arg(0);
  Serial.println("handleFileCreate: " + path);
  if (path == "/")
    return server.send(500, "text/plain", "BAD PATH");
  if (SPIFFS.exists(path))
    return server.send(500, "text/plain", "FILE EXISTS");
  File file = SPIFFS.open(path, "w");
  if (file)
    file.close();
  else
    return server.send(500, "text/plain", "CREATE FAILED");
  server.send(200, "text/plain", "");
  path = String();
}

void handleFileList()
{
  if (!server.hasArg("dir"))
  {
    server.send(500, "text/plain", "BAD ARGS");
    return;
  }

  String path = server.arg("dir");
  Serial.println("handleFileList: " + path);
  Dir dir = SPIFFS.openDir(path);
  path = String();

  String output = "[";
  while (dir.next())
  {
    File entry = dir.openFile("r");
    if (output != "[")
      output += ',';
    bool isDir = false;
    output += "{\"type\":\"";
    output += (isDir) ? "dir" : "file";
    output += "\",\"name\":\"";
    output += String(entry.name()).substring(1);
    output += "\"}";
    entry.close();
  }

  output += "]";
  server.send(200, "text/json", output);
}
