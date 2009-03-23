#include <stdio.h>

#include "sensors/sensors.h"

int main()
{
#if 0
    FILE *fin = fopen("sensors.conf", "r");
    if (fin == NULL)
        perror("No conf file");
    else
    {
        int rc = sensors_init(fin);
        fclose(fin);

        if (rc != 0)
            printf("sensors_init error %d\n", rc);
    }
#endif

    int rc = sensors_init(NULL);
    printf("sensors_init -> %d\n", rc);

    int nr = 0;
    const sensors_chip_name * chip;
    while (
        chip = sensors_get_detected_chips(NULL, &nr),
        chip != NULL)
    {
        printf("Chip: %s %s {%d, %d} \"%s\" (0x%02x)\n",
            chip->prefix, chip->path,
            chip->bus.type, chip->bus.nr,
            sensors_get_adapter_name(&chip->bus),
            chip->addr);

        const sensors_feature * feature;
        int nrf = 0;
        while (
            feature = sensors_get_features(chip, &nrf),
            feature != NULL)
        {
            printf(" Feature: %s %d %d\n",
                feature->name, feature->number, feature->type);

            const sensors_subfeature * sub;
            int nrs = 0;
            while (
                sub = sensors_get_all_subfeatures(chip, feature, &nrs),
                sub != NULL)
            {
                printf("  Sub-feature: %s %d 0x%03x %d 0x%02x, ",
                sub->name, sub->number, sub->type,
                sub->mapping, sub->flags);

                double value;
                rc = sensors_get_value(chip, sub->number, &value);
                if (rc == 0)
                    printf("value = %g\n", value);
                else
                    printf("error = %d\n", rc);
            }
        }
    }
    
    
    return 0;
}
