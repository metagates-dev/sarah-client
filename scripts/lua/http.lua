-- http functions test program
-- @Author	grandhelmsman<desupport@grandhelmsman.com>

print("\n+------ HTTP GET ------+\n")
str, status = http_get("http://grandhelmsman.pro", {
    "accept: text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,image/apng,*/*;q=0.8",
    "user-agent: Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/71.0.3578.98 Safari/537.36"
});
print("status: ", status, "\n");
print("http_get: ", str, "\n");

print("\n+------ HTTP POST ------+\n")
str, status = http_post("http://localhost/post.php", {
    str_key = "value of key1",
    bool_key = true,
    int_key = 1024,
    float_key = 3.14
    }, {
    "accept: text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,image/apng,*/*;q=0.8",
    "user-agent: Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/71.0.3578.98 Safari/537.36"
});
print("status: ", status, "\n");
print("http_post: ", str, "\n");