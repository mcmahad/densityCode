
#include <stdio.h>
#include <stdint.h>


int main(int argc, char** argv)
{
    uint32_t        confirmationNumber,
                    newValidationKey,
                    desiredStickCount,
                    desiredStickIndex = 0;

    printf("\n\n\n\n\n");
    printf("   SinoPro License Key Generator\n\n");
    printf("Enter confirmation number : ");
    if (1 == scanf_s("%d", &confirmationNumber))
    {
        confirmationNumber %= 233000;
        printf("Enter desired stick count : ");
        if (1 == scanf_s("%d", &desiredStickCount))
        {
            uint32_t    index2count[] =
            {
                     10,     20,     50,
                    100,    200,    500,
                   1000,   2000,   5000,
                  10000,  20000,  50000,
                 100000, 200000, 500000,
                1000000
            };

            for (int index = 0; index < sizeof(index2count) / sizeof(index2count[0]); index++)
            {
                if (desiredStickCount <= index2count[index])
                {
                    desiredStickIndex = index;
                    desiredStickCount = index2count[index];
                    break;
                }
            }
            printf("Using stick count of %d\n", desiredStickCount);

            // Create the key
            newValidationKey  = 0;
            newValidationKey  = (confirmationNumber & 0x7FFF) << 16;
            newValidationKey += (                     0x028F) <<  0;
            newValidationKey += (desiredStickIndex  & 0x000F) << 12;
            newValidationKey ^= 0x28F28F;
            printf("\n\n New Update Key : %lu\n\n", newValidationKey);
        }
        else
        {
            printf("Wrong format for input\n");
        }
    }
    else
    {
        printf("No valid confirmation number entered.  No key generated.\n");
    }
    printf("press any key to exit...");
    while (!kbhit());
}

