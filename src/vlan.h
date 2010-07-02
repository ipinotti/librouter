// prototipos de funcoes
int librouter_vlan_exists(int ethernet_no, int vid);
int librouter_vlan_vid(int ethernet_no, int vid, int add_del, int bridge);
int librouter_vlan_set_cos(int ethernet_no, int vid, int cos);
int librouter_vlan_get_cos(char *dev_name);

