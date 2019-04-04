void addQueryParam(String &url, String queryName, String queryValue) {
  if (url.indexOf("?") > 0) {
    url += "&";
  } else {
    url += "?";
  }
  url += queryName;
  url += "=";
  url += queryValue;
}
