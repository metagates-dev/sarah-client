-- create disk partition
disk = "/dev/sdq";
partition = {
    {start=0, blocks=7275390975, format="ext4"},
    {start=7275390976, blocks=-1, format="ext4"}
};
status, errno = sarah_disk_partition(disk, partition);
print("\n---------------------- test result ------------------------------\n");
if (status == true)
then
    print("create partiiton successfully");
else
    print("fail to create partition with errno: ", errno);
end
