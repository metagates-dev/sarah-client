-- for lua file execution testing
--
-- @author  grandhelmsman<desupport@grandhelmsman.com>
--
-- All interface define for executor object
--
-- executor:setMaxDataLen(int)
-- executor:appendData(string)
-- executor:insertData(int, string)
-- executor:clearData()
-- executor:setStatus(int, float)
-- executor:getDataLen()
-- executor:getData()
-- executor:syncStatus()
-- executor:__gc()
-- executor:__tostring()
--

function add(a, b)
    return a + b
end


echo("\n+------ Node Information ------+\n");
local node = executor:getNode();
echo(node, "\n");


-- executor object interface
echo("\n+------ Buffer and status ------+\n");
executor.appendData("This is the output from the lua test script");
executor.setStatus(1, 0.15);
executor.insertData(1, " *INSERT* ");
echo("data from executor buffer: \n", executor:getData());
executor.syncStatus();