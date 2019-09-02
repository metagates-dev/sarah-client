-- image spider demo
-- @author  grandhelmsman<desupport@grandhelmsman.com>
-- 

local page_url = "http://baijiahao.baidu.com/s?id=1606252594175173925&wfr=spider&for=pc"
local headers = {
    'Referer: http://www.baidu.com/link?url=Tba1Vv92wOKus2nGUuj3j-XBqCbNVqvQkXjqw4e1hnwG2zt4PuaFVG9zRRyhRS1Vb5CDDDibJzrvLfRGXF9c9KV35YyIMdZLrEv3sqVMgCK&wd=&eqid=89342fda0006c6de000000065c3d3c33',
    'User-Agent: Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/71.0.3578.98 Safari/537.36'
};

-- local html = [[
-- ]]
local html, status = http_get(page_url, headers);
if ( status == false ) then
    print("Failed to perform the http_get to " .. page_url);
else
    -- Analysis the html content to get all the image links
    local img_src, idx, _count;
    local img_links = {};
    for img_src, idx in string.gmatch(html, "<img[^>]+src=\"([^\"]+)\"") do
        _src, _count = string.gsub(img_src, '&amp;', '&');
        table.insert(img_links, _src);
    end

    -- append the image links to the data buffer
    local json_str, status = json_encode(img_links);
    print(json_str);
end
