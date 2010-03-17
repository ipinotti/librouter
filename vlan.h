// prototipos de funcoes
int vlan_exists(int ethernet_no, int vid);
int vlan_vid(int ethernet_no, int vid, int add_del, int bridge);
int set_vlan_cos(int ethernet_no, int vid, int cos);
int get_vlan_cos(char *dev_name);


