#pragma once
#include <cstring>
#include <strings.h>

// Headerless scripts remain supported. Browser origins must exactly match the
// requested authority; sibling domains, opaque origins and foreign pages fail.
inline bool requestFromOtherSite(const char* origin,const char* site,const char* host) {
  if (site && !strcmp(site,"cross-site")) return true;
  if (!origin || !*origin) return false;
  const char* authority=nullptr;
  if (!strncmp(origin,"http://",7)) authority=origin+7;
  else if (!strncmp(origin,"https://",8)) authority=origin+8;
  if (!authority || !host || !*host || strchr(authority,'/') || strchr(authority,'@')) return true;
  return strcasecmp(authority,host)!=0;
}

struct OtaUploadState {
  bool started=false,complete=false,failed=false;
  void begin() {started=true;complete=false;failed=false;}
  void clear() {started=complete=failed=false;}
  bool canRestart() const {return started && complete && !failed;}
};

inline bool multipartUploadContentType(const char* type) {
  if (!type || strncasecmp(type,"multipart/form-data",19)) return false;
  if (type[19]!=';' && type[19]!=' ') return false;
  const char* boundary=strstr(type+19,"boundary=");
  if (!boundary || !boundary[9] || boundary[9]==';') return false;
  return boundary[9]!='"' || (boundary[10] && boundary[10]!='"');
}
