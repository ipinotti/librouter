// prototipos de funcoes
int libconfig_vlan_exists(int ethernet_no, int vid);
int libconfig_vlan_vid(int ethernet_no, int vid, int add_del, int bridge);
int libconfig_vlan_set_cos(int ethernet_no, int vid, int cos);
int libconfig_vlan_get_cos(char *dev_name);

