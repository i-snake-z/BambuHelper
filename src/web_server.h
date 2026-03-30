#ifndef WEB_SERVER_H
#define WEB_SERVER_H

void initWebServer();
void handleWebServer();
bool        isOtaAutoInProgress();
int         getOtaAutoProgress();
const char* getOtaAutoStatus();

#endif // WEB_SERVER_H
