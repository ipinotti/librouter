typedef struct
{
        unsigned char led_sys;
} ppcio_data;

int read_ppcio(ppcio_data *pd);
int write_ppcio(ppcio_data *pd);


