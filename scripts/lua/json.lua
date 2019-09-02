--
-- lua json encode/decode testing script
-- @Author  grandhelmsman<desupport@grandhelmsman.com>
--

local person = {
    "this is the first line",
    name = "lion",
    age = 28,
    married = true,
    education = {
        "The Number One Middle School Of Anren",
        "Hunan Institude Of Science and Technology"
    },
    child = {
        name = "Joy",
        sex = "femail",
        age = 1
    },
    hobby = {
        {
            name = "Guitar",
            year = 2,
            since = 2016,
            still = true
        },
        {
            name = "Bamboo flute",
            year = 10,
            since = 2008,
            still = true
        },
        {
            name = "vilion",
            year = 2,
            since = 2016,
            still = false,
            will = nil
        }
    }
}

print("\n+------ JSON ENCODE ------+\n")
local str = json_encode(person, false);
print("json_encode: ", str, "\n")

local list = {
    "Guitar",
    "Vilion",
    "Bamboo flute",
    "Bros Harmonica"
}

print("json_encode: ", json_encode(list, false), "\n");




-- lua json decode testing block
print("\n\n+------ JSON DECODE ------+\n")
local str = [[{
    "1": "this is the first line",
    "name": "lion",
    "married": true,
    "child": {
	    "name": "Joy",
	    "age": 1,
	    "sex": "femail"
	},
    "hobby": [{
        "name": "Guitar",
        "since": 2016,
        "still": true,
        "year":	2
    }, {
        "name": "Bamboo flute",
        "since": 2008,
        "still": true,
        "year":	10
    }, {
        "name":	"vilion",
        "since": 2016,
        "still": false,
        "year":	2
    }],
    "age": 28,
    "education": ["The Number One Middle School Of Anren", "Hunan Institude Of Science and Technology"]
}]]

local json, status = json_decode(str)
print("str 01: \n")
print("status: ", status, "\n")
print("Table JSON: ", json_encode(json), "\n")


str = [[
[
    "first line",
    "second line",
    "third line"
]
]]
print("\nstr 02: \n")
json, status = json_decode(str)
print("status: ", status, "\n")
print("Table JSON: ", json_encode(json), "\n")


str = [[
{
    "lines": [
        "first line",
        "second line",
        "third line"
    ]
}
]]
print("\nstr 03: \n")
json, status = json_decode(str)
print("status: ", status, "\n");
print("Table JSON: ", json_encode(json), "\n");