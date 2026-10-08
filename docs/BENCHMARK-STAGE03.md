# Stage03 — benchmark CPU înainte / după

192 kHz, toate cele 16 ieșiri active în FIR, MSVC x64 / Intel IPP 2026. Steady: 10.000 măsurători/rând; tranziții: 200 adoptări/rând. QPC include scheduling; ciclurile sunt costul thread-ului plus wrapper, fără conversie în timp. Condițiile și limitele sunt în [raportul principal](VALIDATION-STAGE03.md).

## Timp QPC în µs

| Modul | Program | Configurație | Frames | Înainte median/p99/max µs | După median/p99/max µs | Buget µs | Înainte | După |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Crossover | MUSIC | unity | 128 | 51.40 / 135.50 / 335.50 | 52.50 / 136.20 / 558.10 | 666.67 | PASS | PASS |
| OutputMatrix | MUSIC | unity | 128 | 3.90 / 20.60 / 213.80 | 2.80 / 17.00 / 97.80 | 666.67 | PASS | PASS |
| Headroom | MUSIC | unity | 128 | 3.80 / 21.80 / 238.90 | 3.80 / 20.30 / 210.00 | 666.67 | PASS | PASS |
| AudioGraph | MUSIC | unity | 128 | 62.50 / 156.00 / 521.40 | 62.50 / 160.00 / 594.20 | 666.67 | PASS | PASS |
| Crossover | MUSIC | IIR-AP | 128 | 75.20 / 175.80 / 379.50 | 75.10 / 169.50 / 338.10 | 666.67 | PASS | PASS |
| OutputMatrix | MUSIC | IIR-AP | 128 | 3.90 / 20.60 / 209.60 | 2.80 / 19.10 / 139.90 | 666.67 | PASS | PASS |
| Headroom | MUSIC | IIR-AP | 128 | 3.80 / 20.20 / 307.00 | 3.80 / 21.80 / 364.90 | 666.67 | PASS | PASS |
| AudioGraph | MUSIC | IIR-AP | 128 | 86.00 / 199.80 / 443.20 | 84.45 / 179.70 / 547.90 | 666.67 | PASS | PASS |
| Crossover | MUSIC | FIR8193 | 128 | 601.00 / 1715.10 / 3148.50 | 362.65 / 1129.70 / 2885.20 | 666.67 | FAIL | FAIL |
| OutputMatrix | MUSIC | FIR8193 | 128 | 3.90 / 20.60 / 254.90 | 2.80 / 18.30 / 128.40 | 666.67 | PASS | PASS |
| Headroom | MUSIC | FIR8193 | 128 | 3.80 / 19.30 / 130.60 | 3.80 / 22.80 / 190.60 | 666.67 | PASS | PASS |
| AudioGraph | MUSIC | FIR8193 | 128 | 597.60 / 1683.90 / 5725.80 | 380.35 / 1185.70 / 2983.80 | 666.67 | FAIL | FAIL |
| Crossover | MUSIC | FIR8193-AP | 128 | 593.50 / 1615.00 / 4065.30 | 368.60 / 1132.20 / 3460.90 | 666.67 | FAIL | FAIL |
| OutputMatrix | MUSIC | FIR8193-AP | 128 | 3.90 / 19.30 / 85.80 | 2.80 / 17.70 / 142.80 | 666.67 | PASS | PASS |
| Headroom | MUSIC | FIR8193-AP | 128 | 3.80 / 20.20 / 136.50 | 3.80 / 20.00 / 93.00 | 666.67 | PASS | PASS |
| AudioGraph | MUSIC | FIR8193-AP | 128 | 645.65 / 1786.40 / 3324.50 | 381.05 / 1112.40 / 3326.30 | 666.67 | FAIL | FAIL |
| Crossover | MUSIC | filter-transition | 128 | 2313.85 / 4267.00 / 4783.50 | 1663.25 / 2982.60 / 3380.20 | 666.67 | FAIL | FAIL |
| AudioGraph | MUSIC | filter-transition | 128 | 2329.60 / 4255.40 / 7649.10 | 1727.50 / 2878.90 / 3027.70 | 666.67 | FAIL | FAIL |
| Crossover | MUSIC | emergency-mute | 128 | 3499.80 / 9309.20 / 10223.80 | 137.50 / 381.90 / 436.40 | 666.67 | FAIL | PASS |
| AudioGraph | MUSIC | emergency-mute | 128 | 3039.15 / 9914.60 / 52485.40 | 182.70 / 399.30 / 450.00 | 666.67 | FAIL | PASS |
| Crossover | MUSIC | topology-disable | 128 | 3097.15 / 10095.50 / 10817.80 | 143.80 / 424.50 / 467.60 | 666.67 | FAIL | PASS |
| AudioGraph | MUSIC | topology-disable | 128 | 3142.45 / 8936.30 / 10272.60 | 170.95 / 554.30 / 595.70 | 666.67 | FAIL | PASS |
| Crossover | MUSIC | IIR-FIR-transition | 128 | 1474.85 / 3282.50 / 3379.20 | 326.25 / 952.60 / 1124.80 | 666.67 | FAIL | FAIL |
| AudioGraph | MUSIC | IIR-FIR-transition | 128 | 1538.05 / 4006.90 / 4559.40 | 325.25 / 803.50 / 1113.80 | 666.67 | FAIL | FAIL |
| Crossover | MUSIC | FIR-length-transition | 128 | 1586.25 / 3310.10 / 3798.60 | 1083.40 / 2281.90 / 2403.30 | 666.67 | FAIL | FAIL |
| AudioGraph | MUSIC | FIR-length-transition | 128 | 1651.20 / 3730.10 / 4092.90 | 1161.25 / 2255.00 / 3053.30 | 666.67 | FAIL | FAIL |
| Crossover | MUSIC | program-transition | 128 | 2429.35 / 4103.00 / 4417.00 | 1631.80 / 3132.20 / 3902.70 | 666.67 | FAIL | FAIL |
| AudioGraph | MUSIC | program-transition | 128 | 2427.70 / 4093.70 / 5160.30 | 1739.15 / 3217.30 / 3514.80 | 666.67 | FAIL | FAIL |
| Crossover | MUSIC | unity | 256 | 100.60 / 229.40 / 1492.50 | 102.70 / 216.10 / 570.10 | 1333.33 | FAIL | PASS |
| OutputMatrix | MUSIC | unity | 256 | 7.70 / 31.90 / 159.00 | 5.50 / 24.50 / 137.10 | 1333.33 | PASS | PASS |
| Headroom | MUSIC | unity | 256 | 7.50 / 32.80 / 166.50 | 7.50 / 29.00 / 218.80 | 1333.33 | PASS | PASS |
| AudioGraph | MUSIC | unity | 256 | 121.10 / 254.60 / 602.10 | 120.90 / 248.10 / 555.00 | 1333.33 | PASS | PASS |
| Crossover | MUSIC | IIR-AP | 256 | 147.80 / 291.90 / 663.60 | 148.00 / 284.30 / 698.50 | 1333.33 | PASS | PASS |
| OutputMatrix | MUSIC | IIR-AP | 256 | 7.70 / 30.00 / 423.10 | 5.50 / 24.40 / 111.30 | 1333.33 | PASS | PASS |
| Headroom | MUSIC | IIR-AP | 256 | 7.40 / 29.10 / 104.50 | 7.50 / 30.50 / 147.70 | 1333.33 | PASS | PASS |
| AudioGraph | MUSIC | IIR-AP | 256 | 169.90 / 338.70 / 1905.80 | 166.50 / 337.20 / 1161.00 | 1333.33 | FAIL | PASS |
| Crossover | MUSIC | FIR8193 | 256 | 899.55 / 2150.50 / 3777.30 | 619.30 / 1521.80 / 3149.30 | 1333.33 | FAIL | FAIL |
| OutputMatrix | MUSIC | FIR8193 | 256 | 7.70 / 30.40 / 155.10 | 5.50 / 24.80 / 573.50 | 1333.33 | PASS | PASS |
| Headroom | MUSIC | FIR8193 | 256 | 7.50 / 30.70 / 208.20 | 7.50 / 28.00 / 301.80 | 1333.33 | PASS | PASS |
| AudioGraph | MUSIC | FIR8193 | 256 | 974.90 / 2369.00 / 6603.70 | 682.00 / 1915.60 / 33261.00 | 1333.33 | FAIL | FAIL |
| Crossover | MUSIC | FIR8193-AP | 256 | 935.75 / 2296.40 / 4138.00 | 737.60 / 2058.90 / 5757.80 | 1333.33 | FAIL | FAIL |
| OutputMatrix | MUSIC | FIR8193-AP | 256 | 7.70 / 32.50 / 557.60 | 5.50 / 24.90 / 136.00 | 1333.33 | PASS | PASS |
| Headroom | MUSIC | FIR8193-AP | 256 | 7.40 / 33.30 / 226.90 | 7.60 / 32.30 / 192.50 | 1333.33 | PASS | PASS |
| AudioGraph | MUSIC | FIR8193-AP | 256 | 954.40 / 2262.40 / 3726.70 | 785.00 / 1924.40 / 4162.10 | 1333.33 | FAIL | FAIL |
| Crossover | MUSIC | filter-transition | 256 | 2862.85 / 5172.50 / 5310.90 | 1974.15 / 3581.50 / 18069.60 | 1333.33 | FAIL | FAIL |
| AudioGraph | MUSIC | filter-transition | 256 | 2965.10 / 4650.70 / 6629.30 | 2002.20 / 3792.90 / 5454.20 | 1333.33 | FAIL | FAIL |
| Crossover | MUSIC | emergency-mute | 256 | 3503.05 / 9602.90 / 10691.70 | 205.85 / 585.40 / 621.00 | 1333.33 | FAIL | PASS |
| AudioGraph | MUSIC | emergency-mute | 256 | 3498.15 / 9573.20 / 11721.30 | 256.75 / 577.20 / 647.80 | 1333.33 | FAIL | PASS |
| Crossover | MUSIC | topology-disable | 256 | 3574.15 / 9888.90 / 10164.60 | 234.50 / 763.50 / 782.80 | 1333.33 | FAIL | PASS |
| AudioGraph | MUSIC | topology-disable | 256 | 3721.10 / 7939.20 / 8652.30 | 269.00 / 796.90 / 1040.80 | 1333.33 | FAIL | PASS |
| Crossover | MUSIC | IIR-FIR-transition | 256 | 1869.65 / 3552.50 / 4015.10 | 578.80 / 1248.00 / 1504.20 | 1333.33 | FAIL | FAIL |
| AudioGraph | MUSIC | IIR-FIR-transition | 256 | 1829.30 / 3967.60 / 4508.80 | 606.50 / 1548.20 / 1585.40 | 1333.33 | FAIL | FAIL |
| Crossover | MUSIC | FIR-length-transition | 256 | 2077.25 / 3614.80 / 4327.60 | 1442.20 / 2631.30 / 2963.50 | 1333.33 | FAIL | FAIL |
| AudioGraph | MUSIC | FIR-length-transition | 256 | 1990.90 / 4094.50 / 5028.10 | 1461.10 / 2646.70 / 3016.60 | 1333.33 | FAIL | FAIL |
| Crossover | MUSIC | program-transition | 256 | 2949.05 / 4833.40 / 5130.90 | 2150.20 / 3958.50 / 4505.40 | 1333.33 | FAIL | FAIL |
| AudioGraph | MUSIC | program-transition | 256 | 3030.75 / 4660.80 / 5044.90 | 2060.15 / 3932.00 / 5510.00 | 1333.33 | FAIL | FAIL |
| Crossover | MUSIC | unity | 512 | 208.45 / 400.90 / 1676.60 | 204.40 / 387.70 / 1199.50 | 2666.67 | PASS | PASS |
| OutputMatrix | MUSIC | unity | 512 | 15.00 / 56.50 / 318.80 | 11.10 / 36.50 / 179.20 | 2666.67 | PASS | PASS |
| Headroom | MUSIC | unity | 512 | 14.90 / 46.60 / 243.80 | 14.80 / 46.00 / 578.60 | 2666.67 | PASS | PASS |
| AudioGraph | MUSIC | unity | 512 | 246.75 / 474.20 / 43343.40 | 242.40 / 438.70 / 1951.60 | 2666.67 | FAIL | PASS |
| Crossover | MUSIC | IIR-AP | 512 | 308.70 / 516.70 / 1762.40 | 312.50 / 547.80 / 2891.70 | 2666.67 | PASS | FAIL |
| OutputMatrix | MUSIC | IIR-AP | 512 | 15.00 / 47.00 / 227.20 | 11.10 / 37.10 / 267.70 | 2666.67 | PASS | PASS |
| Headroom | MUSIC | IIR-AP | 512 | 14.80 / 47.60 / 198.90 | 15.00 / 44.50 / 230.40 | 2666.67 | PASS | PASS |
| AudioGraph | MUSIC | IIR-AP | 512 | 362.70 / 590.90 / 1051.20 | 363.00 / 595.20 / 2037.30 | 2666.67 | PASS | PASS |
| Crossover | MUSIC | FIR8193 | 512 | 1629.20 / 3213.20 / 5552.70 | 1154.90 / 2328.20 / 3722.20 | 2666.67 | FAIL | FAIL |
| OutputMatrix | MUSIC | FIR8193 | 512 | 15.50 / 60.00 / 487.80 | 11.10 / 38.10 / 208.20 | 2666.67 | PASS | PASS |
| Headroom | MUSIC | FIR8193 | 512 | 14.70 / 47.20 / 357.60 | 14.70 / 45.20 / 630.30 | 2666.67 | PASS | PASS |
| AudioGraph | MUSIC | FIR8193 | 512 | 1704.90 / 3422.90 / 12748.40 | 1219.20 / 2457.50 / 5125.80 | 2666.67 | FAIL | FAIL |
| Crossover | MUSIC | FIR8193-AP | 512 | 1620.10 / 3184.50 / 4550.60 | 1167.40 / 2360.40 / 19196.60 | 2666.67 | FAIL | FAIL |
| OutputMatrix | MUSIC | FIR8193-AP | 512 | 14.90 / 55.00 / 199.10 | 11.10 / 37.30 / 651.60 | 2666.67 | PASS | PASS |
| Headroom | MUSIC | FIR8193-AP | 512 | 14.80 / 47.90 / 325.30 | 14.80 / 45.90 / 283.10 | 2666.67 | PASS | PASS |
| AudioGraph | MUSIC | FIR8193-AP | 512 | 1708.25 / 3393.80 / 33189.10 | 1231.20 / 2506.20 / 3894.80 | 2666.67 | FAIL | FAIL |
| Crossover | MUSIC | filter-transition | 512 | 4031.30 / 5920.80 / 6587.70 | 2894.30 / 4696.50 / 4899.00 | 2666.67 | FAIL | FAIL |
| AudioGraph | MUSIC | filter-transition | 512 | 4219.65 / 6031.30 / 6418.80 | 2882.55 / 4605.90 / 5133.30 | 2666.67 | FAIL | FAIL |
| Crossover | MUSIC | emergency-mute | 512 | 3787.20 / 9118.10 / 10154.20 | 327.60 / 939.60 / 1409.00 | 2666.67 | FAIL | PASS |
| AudioGraph | MUSIC | emergency-mute | 512 | 3837.95 / 9040.50 / 11227.10 | 480.20 / 1037.40 / 1168.90 | 2666.67 | FAIL | PASS |
| Crossover | MUSIC | topology-disable | 512 | 3682.30 / 8011.40 / 10735.60 | 417.50 / 1322.30 / 1401.00 | 2666.67 | FAIL | PASS |
| AudioGraph | MUSIC | topology-disable | 512 | 3797.35 / 9468.50 / 10097.10 | 472.35 / 1270.30 / 1398.30 | 2666.67 | FAIL | PASS |
| Crossover | MUSIC | IIR-FIR-transition | 512 | 2540.75 / 4856.10 / 5393.70 | 1105.05 / 2687.60 / 3154.90 | 2666.67 | FAIL | FAIL |
| AudioGraph | MUSIC | IIR-FIR-transition | 512 | 2515.25 / 4634.80 / 5891.30 | 1138.45 / 2664.60 / 3133.80 | 2666.67 | FAIL | FAIL |
| Crossover | MUSIC | FIR-length-transition | 512 | 2710.95 / 4395.30 / 4477.00 | 2170.55 / 3341.00 / 3709.20 | 2666.67 | FAIL | FAIL |
| AudioGraph | MUSIC | FIR-length-transition | 512 | 2822.50 / 4819.40 / 5204.30 | 2109.05 / 3374.70 / 4207.40 | 2666.67 | FAIL | FAIL |
| Crossover | MUSIC | program-transition | 512 | 4085.80 / 6269.10 / 9952.10 | 2946.80 / 4516.10 / 4939.10 | 2666.67 | FAIL | FAIL |
| AudioGraph | MUSIC | program-transition | 512 | 4184.00 / 5989.10 / 6436.70 | 3073.60 / 5013.10 / 5947.90 | 2666.67 | FAIL | FAIL |
| Crossover | MUSIC | unity | 1024 | 435.40 / 691.80 / 2145.50 | 458.90 / 752.40 / 37106.40 | 5333.33 | PASS | FAIL |
| OutputMatrix | MUSIC | unity | 1024 | 30.20 / 92.00 / 301.20 | 23.30 / 59.60 / 252.60 | 5333.33 | PASS | PASS |
| Headroom | MUSIC | unity | 1024 | 29.70 / 78.30 / 348.40 | 29.80 / 77.70 / 302.60 | 5333.33 | PASS | PASS |
| AudioGraph | MUSIC | unity | 1024 | 520.70 / 808.40 / 1422.60 | 521.10 / 828.80 / 1588.50 | 5333.33 | PASS | PASS |
| Crossover | MUSIC | IIR-AP | 1024 | 631.30 / 966.10 / 3081.80 | 637.50 / 972.20 / 4836.20 | 5333.33 | PASS | PASS |
| OutputMatrix | MUSIC | IIR-AP | 1024 | 30.20 / 85.80 / 633.20 | 23.30 / 67.40 / 607.90 | 5333.33 | PASS | PASS |
| Headroom | MUSIC | IIR-AP | 1024 | 30.30 / 98.30 / 1467.90 | 30.30 / 85.30 / 530.60 | 5333.33 | PASS | PASS |
| AudioGraph | MUSIC | IIR-AP | 1024 | 742.45 / 1155.10 / 2569.30 | 710.70 / 1089.70 / 1564.30 | 5333.33 | PASS | PASS |
| Crossover | MUSIC | FIR8193 | 1024 | 2907.05 / 4692.40 / 26520.40 | 2165.50 / 3555.40 / 5764.30 | 5333.33 | FAIL | FAIL |
| OutputMatrix | MUSIC | FIR8193 | 1024 | 30.10 / 74.20 / 273.60 | 23.40 / 62.90 / 307.90 | 5333.33 | PASS | PASS |
| Headroom | MUSIC | FIR8193 | 1024 | 29.70 / 82.60 / 644.50 | 29.50 / 74.30 / 296.80 | 5333.33 | PASS | PASS |
| AudioGraph | MUSIC | FIR8193 | 1024 | 3055.65 / 4832.90 / 7402.80 | 2333.10 / 4025.90 / 41009.60 | 5333.33 | FAIL | FAIL |
| Crossover | MUSIC | FIR8193-AP | 1024 | 2953.95 / 4803.60 / 38767.50 | 2195.35 / 3679.20 / 6091.30 | 5333.33 | FAIL | FAIL |
| OutputMatrix | MUSIC | FIR8193-AP | 1024 | 30.00 / 75.10 / 644.50 | 23.40 / 61.50 / 1185.10 | 5333.33 | PASS | PASS |
| Headroom | MUSIC | FIR8193-AP | 1024 | 29.40 / 80.70 / 672.00 | 29.50 / 82.20 / 619.40 | 5333.33 | PASS | PASS |
| AudioGraph | MUSIC | FIR8193-AP | 1024 | 3073.50 / 4915.90 / 8498.20 | 2329.60 / 3913.00 / 14618.60 | 5333.33 | FAIL | FAIL |
| Crossover | MUSIC | filter-transition | 1024 | 5952.25 / 7819.70 / 10653.50 | 4441.10 / 6342.00 / 8248.50 | 5333.33 | FAIL | FAIL |
| AudioGraph | MUSIC | filter-transition | 1024 | 6164.25 / 8528.70 / 8917.80 | 4571.35 / 6224.70 / 7076.50 | 5333.33 | FAIL | FAIL |
| Crossover | MUSIC | emergency-mute | 1024 | 4273.10 / 9303.30 / 10757.70 | 679.10 / 1914.50 / 2257.90 | 5333.33 | FAIL | PASS |
| AudioGraph | MUSIC | emergency-mute | 1024 | 4580.90 / 10063.70 / 11172.70 | 875.75 / 1895.40 / 2000.00 | 5333.33 | FAIL | PASS |
| Crossover | MUSIC | topology-disable | 1024 | 4504.45 / 10250.50 / 12080.20 | 1249.05 / 2056.80 / 2098.50 | 5333.33 | FAIL | PASS |
| AudioGraph | MUSIC | topology-disable | 1024 | 4575.60 / 10408.40 / 10736.00 | 934.65 / 2178.70 / 2234.00 | 5333.33 | FAIL | PASS |
| Crossover | MUSIC | IIR-FIR-transition | 1024 | 3806.35 / 5564.50 / 7142.70 | 2056.25 / 3705.50 / 3943.20 | 5333.33 | FAIL | PASS |
| AudioGraph | MUSIC | IIR-FIR-transition | 1024 | 3950.10 / 6521.80 / 6824.90 | 2290.75 / 4258.30 / 4392.70 | 5333.33 | FAIL | PASS |
| Crossover | MUSIC | FIR-length-transition | 1024 | 4396.05 / 6335.20 / 6943.90 | 3241.15 / 4877.40 / 5254.10 | 5333.33 | FAIL | PASS |
| AudioGraph | MUSIC | FIR-length-transition | 1024 | 4544.75 / 7617.80 / 25396.00 | 3349.90 / 4362.70 / 4534.40 | 5333.33 | FAIL | PASS |
| Crossover | MUSIC | program-transition | 1024 | 6258.65 / 8620.10 / 9929.20 | 4673.80 / 6501.40 / 7082.40 | 5333.33 | FAIL | FAIL |
| AudioGraph | MUSIC | program-transition | 1024 | 6411.70 / 8646.30 / 9347.10 | 4731.20 / 6733.70 / 8138.60 | 5333.33 | FAIL | FAIL |
| Crossover | CINEMA | unity | 128 | 51.90 / 128.10 / 324.00 | 52.70 / 129.90 / 355.90 | 666.67 | PASS | PASS |
| OutputMatrix | CINEMA | unity | 128 | 3.90 / 19.80 / 156.90 | 2.90 / 20.20 / 94.60 | 666.67 | PASS | PASS |
| Headroom | CINEMA | unity | 128 | 3.90 / 19.80 / 166.70 | 3.80 / 20.70 / 147.30 | 666.67 | PASS | PASS |
| AudioGraph | CINEMA | unity | 128 | 62.40 / 161.90 / 382.30 | 61.80 / 146.30 / 374.50 | 666.67 | PASS | PASS |
| Crossover | CINEMA | IIR-AP | 128 | 75.40 / 174.30 / 482.30 | 75.20 / 171.90 / 399.90 | 666.67 | PASS | PASS |
| OutputMatrix | CINEMA | IIR-AP | 128 | 3.90 / 18.70 / 113.20 | 2.80 / 19.00 / 155.00 | 666.67 | PASS | PASS |
| Headroom | CINEMA | IIR-AP | 128 | 3.80 / 19.90 / 128.00 | 3.80 / 20.90 / 266.10 | 666.67 | PASS | PASS |
| AudioGraph | CINEMA | IIR-AP | 128 | 86.00 / 196.30 / 509.50 | 84.90 / 179.80 / 444.50 | 666.67 | PASS | PASS |
| Crossover | CINEMA | FIR8193 | 128 | 539.60 / 1590.70 / 2620.30 | 381.95 / 1227.20 / 2451.40 | 666.67 | FAIL | FAIL |
| OutputMatrix | CINEMA | FIR8193 | 128 | 3.90 / 20.90 / 147.20 | 2.80 / 18.10 / 182.00 | 666.67 | PASS | PASS |
| Headroom | CINEMA | FIR8193 | 128 | 3.80 / 19.90 / 98.60 | 3.80 / 22.90 / 143.40 | 666.67 | PASS | PASS |
| AudioGraph | CINEMA | FIR8193 | 128 | 603.20 / 1714.80 / 2853.20 | 378.20 / 1142.90 / 2277.70 | 666.67 | FAIL | FAIL |
| Crossover | CINEMA | FIR8193-AP | 128 | 672.30 / 1859.00 / 3561.40 | 354.50 / 1089.10 / 3296.90 | 666.67 | FAIL | FAIL |
| OutputMatrix | CINEMA | FIR8193-AP | 128 | 4.10 / 21.10 / 185.30 | 2.80 / 17.30 / 139.90 | 666.67 | PASS | PASS |
| Headroom | CINEMA | FIR8193-AP | 128 | 4.00 / 20.40 / 267.80 | 3.80 / 20.60 / 112.70 | 666.67 | PASS | PASS |
| AudioGraph | CINEMA | FIR8193-AP | 128 | 839.30 / 2316.40 / 34659.20 | 381.90 / 1151.30 / 3228.70 | 666.67 | FAIL | FAIL |
| Crossover | CINEMA | filter-transition | 128 | 2584.15 / 4428.70 / 5324.40 | 1654.30 / 3286.20 / 8881.00 | 666.67 | FAIL | FAIL |
| AudioGraph | CINEMA | filter-transition | 128 | 2436.35 / 4383.70 / 10020.90 | 1752.85 / 3008.40 / 3708.70 | 666.67 | FAIL | FAIL |
| Crossover | CINEMA | emergency-mute | 128 | 3129.65 / 8735.20 / 9306.30 | 137.85 / 358.10 / 1721.10 | 666.67 | FAIL | FAIL |
| AudioGraph | CINEMA | emergency-mute | 128 | 3489.65 / 10853.80 / 12806.00 | 168.85 / 416.80 / 540.30 | 666.67 | FAIL | PASS |
| Crossover | CINEMA | topology-disable | 128 | 3035.25 / 9108.30 / 10149.40 | 188.70 / 473.70 / 902.60 | 666.67 | FAIL | FAIL |
| AudioGraph | CINEMA | topology-disable | 128 | 3502.75 / 9001.40 / 9092.20 | 161.05 / 535.80 / 591.90 | 666.67 | FAIL | PASS |
| Crossover | CINEMA | IIR-FIR-transition | 128 | 1513.85 / 3550.90 / 3708.80 | 337.05 / 1125.10 / 1426.40 | 666.67 | FAIL | FAIL |
| AudioGraph | CINEMA | IIR-FIR-transition | 128 | 1529.55 / 3180.90 / 4477.90 | 345.75 / 975.00 / 1252.40 | 666.67 | FAIL | FAIL |
| Crossover | CINEMA | FIR-length-transition | 128 | 1579.45 / 3294.80 / 4167.30 | 1059.25 / 2377.20 / 5849.90 | 666.67 | FAIL | FAIL |
| AudioGraph | CINEMA | FIR-length-transition | 128 | 1596.65 / 3011.70 / 4309.40 | 1090.15 / 2527.90 / 3146.20 | 666.67 | FAIL | FAIL |
| Crossover | CINEMA | program-transition | 128 | 2504.85 / 4693.20 / 4831.90 | 1698.45 / 3174.40 / 3604.00 | 666.67 | FAIL | FAIL |
| AudioGraph | CINEMA | program-transition | 128 | 2569.85 / 4575.00 / 5152.30 | 1727.05 / 3187.50 / 3733.20 | 666.67 | FAIL | FAIL |
| Crossover | CINEMA | unity | 256 | 101.40 / 221.40 / 487.70 | 102.40 / 222.60 / 588.20 | 1333.33 | PASS | PASS |
| OutputMatrix | CINEMA | unity | 256 | 7.70 / 29.20 / 150.50 | 5.50 / 24.00 / 160.70 | 1333.33 | PASS | PASS |
| Headroom | CINEMA | unity | 256 | 7.40 / 30.00 / 258.10 | 7.40 / 29.30 / 238.80 | 1333.33 | PASS | PASS |
| AudioGraph | CINEMA | unity | 256 | 121.90 / 266.20 / 584.60 | 120.90 / 247.90 / 623.70 | 1333.33 | PASS | PASS |
| Crossover | CINEMA | IIR-AP | 256 | 148.50 / 303.90 / 644.00 | 148.00 / 288.30 / 537.90 | 1333.33 | PASS | PASS |
| OutputMatrix | CINEMA | IIR-AP | 256 | 7.60 / 30.60 / 191.00 | 5.50 / 23.90 / 118.10 | 1333.33 | PASS | PASS |
| Headroom | CINEMA | IIR-AP | 256 | 7.40 / 30.30 / 285.50 | 7.40 / 29.10 / 177.80 | 1333.33 | PASS | PASS |
| AudioGraph | CINEMA | IIR-AP | 256 | 170.40 / 332.30 / 694.20 | 166.00 / 332.70 / 1906.50 | 1333.33 | PASS | FAIL |
| Crossover | CINEMA | FIR8193 | 256 | 891.65 / 2259.20 / 4136.90 | 624.40 / 1574.40 / 3106.50 | 1333.33 | FAIL | FAIL |
| OutputMatrix | CINEMA | FIR8193 | 256 | 7.70 / 31.50 / 560.30 | 5.50 / 23.50 / 174.20 | 1333.33 | PASS | PASS |
| Headroom | CINEMA | FIR8193 | 256 | 7.50 / 29.20 / 202.20 | 7.40 / 30.90 / 120.20 | 1333.33 | PASS | PASS |
| AudioGraph | CINEMA | FIR8193 | 256 | 960.20 / 2336.00 / 4287.40 | 655.90 / 1575.10 / 4348.90 | 1333.33 | FAIL | FAIL |
| Crossover | CINEMA | FIR8193-AP | 256 | 941.20 / 2257.80 / 4285.00 | 648.10 / 1558.00 / 2886.50 | 1333.33 | FAIL | FAIL |
| OutputMatrix | CINEMA | FIR8193-AP | 256 | 7.60 / 29.00 / 165.10 | 5.50 / 24.00 / 133.50 | 1333.33 | PASS | PASS |
| Headroom | CINEMA | FIR8193-AP | 256 | 7.50 / 33.40 / 398.80 | 7.50 / 27.90 / 168.80 | 1333.33 | PASS | PASS |
| AudioGraph | CINEMA | FIR8193-AP | 256 | 970.20 / 2338.30 / 5154.90 | 679.65 / 1652.20 / 7422.10 | 1333.33 | FAIL | FAIL |
| Crossover | CINEMA | filter-transition | 256 | 2924.95 / 5083.80 / 5450.60 | 2134.30 / 3619.30 / 4144.80 | 1333.33 | FAIL | FAIL |
| AudioGraph | CINEMA | filter-transition | 256 | 2982.95 / 4843.20 / 5196.80 | 2101.35 / 4093.70 / 5393.90 | 1333.33 | FAIL | FAIL |
| Crossover | CINEMA | emergency-mute | 256 | 3213.35 / 8949.30 / 9505.50 | 215.75 / 454.90 / 843.30 | 1333.33 | FAIL | PASS |
| AudioGraph | CINEMA | emergency-mute | 256 | 3160.30 / 9293.60 / 11102.50 | 254.70 / 689.60 / 785.60 | 1333.33 | FAIL | PASS |
| Crossover | CINEMA | topology-disable | 256 | 3454.40 / 8946.80 / 11868.70 | 230.60 / 744.60 / 962.20 | 1333.33 | FAIL | PASS |
| AudioGraph | CINEMA | topology-disable | 256 | 3479.45 / 7800.20 / 8358.80 | 257.60 / 824.70 / 880.20 | 1333.33 | FAIL | PASS |
| Crossover | CINEMA | IIR-FIR-transition | 256 | 1788.30 / 4389.10 / 4948.20 | 566.50 / 1683.10 / 1938.80 | 1333.33 | FAIL | FAIL |
| AudioGraph | CINEMA | IIR-FIR-transition | 256 | 1807.90 / 3453.50 / 3739.50 | 609.65 / 1335.00 / 1654.10 | 1333.33 | FAIL | FAIL |
| Crossover | CINEMA | FIR-length-transition | 256 | 1991.50 / 3670.20 / 4249.20 | 1402.70 / 2616.20 / 2731.20 | 1333.33 | FAIL | FAIL |
| AudioGraph | CINEMA | FIR-length-transition | 256 | 1968.00 / 3552.90 / 4543.30 | 1481.80 / 2853.50 / 3398.60 | 1333.33 | FAIL | FAIL |
| Crossover | CINEMA | program-transition | 256 | 3137.20 / 4603.80 / 4999.90 | 2152.25 / 3439.50 / 3723.90 | 1333.33 | FAIL | FAIL |
| AudioGraph | CINEMA | program-transition | 256 | 3012.50 / 4938.10 / 6329.40 | 2175.50 / 3715.50 / 4128.60 | 1333.33 | FAIL | FAIL |
| Crossover | CINEMA | unity | 512 | 201.50 / 400.30 / 1838.40 | 203.70 / 374.30 / 905.00 | 2666.67 | PASS | PASS |
| OutputMatrix | CINEMA | unity | 512 | 15.00 / 46.40 / 157.90 | 11.10 / 35.40 / 316.50 | 2666.67 | PASS | PASS |
| Headroom | CINEMA | unity | 512 | 14.70 / 48.90 / 591.60 | 15.10 / 45.40 / 262.60 | 2666.67 | PASS | PASS |
| AudioGraph | CINEMA | unity | 512 | 244.50 / 461.70 / 1931.40 | 261.30 / 452.80 / 1744.60 | 2666.67 | PASS | PASS |
| Crossover | CINEMA | IIR-AP | 512 | 309.30 / 525.20 / 962.50 | 321.35 / 544.80 / 3400.40 | 2666.67 | PASS | FAIL |
| OutputMatrix | CINEMA | IIR-AP | 512 | 15.00 / 48.40 / 209.30 | 11.60 / 31.50 / 193.60 | 2666.67 | PASS | PASS |
| Headroom | CINEMA | IIR-AP | 512 | 14.80 / 46.00 / 267.20 | 14.80 / 43.00 / 212.80 | 2666.67 | PASS | PASS |
| AudioGraph | CINEMA | IIR-AP | 512 | 365.90 / 595.00 / 2073.40 | 364.20 / 580.00 / 1028.10 | 2666.67 | PASS | PASS |
| Crossover | CINEMA | FIR8193 | 512 | 1588.65 / 3084.10 / 4718.80 | 1156.15 / 2267.40 / 4337.80 | 2666.67 | FAIL | FAIL |
| OutputMatrix | CINEMA | FIR8193 | 512 | 14.90 / 46.20 / 224.40 | 11.10 / 36.30 / 361.50 | 2666.67 | PASS | PASS |
| Headroom | CINEMA | FIR8193 | 512 | 14.70 / 43.90 / 622.60 | 14.70 / 43.80 / 158.20 | 2666.67 | PASS | PASS |
| AudioGraph | CINEMA | FIR8193 | 512 | 1694.00 / 3299.60 / 5488.40 | 1202.20 / 2424.20 / 4128.80 | 2666.67 | FAIL | FAIL |
| Crossover | CINEMA | FIR8193-AP | 512 | 1629.25 / 3163.00 / 5458.00 | 1157.10 / 2258.40 / 6029.80 | 2666.67 | FAIL | FAIL |
| OutputMatrix | CINEMA | FIR8193-AP | 512 | 14.90 / 45.30 / 375.80 | 11.10 / 37.40 / 207.40 | 2666.67 | PASS | PASS |
| Headroom | CINEMA | FIR8193-AP | 512 | 14.80 / 48.60 / 190.50 | 14.80 / 43.90 / 258.60 | 2666.67 | PASS | PASS |
| AudioGraph | CINEMA | FIR8193-AP | 512 | 1676.05 / 3237.80 / 5297.70 | 1242.40 / 2642.00 / 46383.40 | 2666.67 | FAIL | FAIL |
| Crossover | CINEMA | filter-transition | 512 | 3991.05 / 6395.00 / 7186.30 | 2837.55 / 4152.80 / 4934.10 | 2666.67 | FAIL | FAIL |
| AudioGraph | CINEMA | filter-transition | 512 | 4104.60 / 6807.20 / 7451.40 | 2928.95 / 4372.60 / 6891.80 | 2666.67 | FAIL | FAIL |
| Crossover | CINEMA | emergency-mute | 512 | 3805.05 / 9472.40 / 10488.70 | 344.45 / 1110.20 / 2434.60 | 2666.67 | FAIL | PASS |
| AudioGraph | CINEMA | emergency-mute | 512 | 3845.40 / 10041.10 / 10572.40 | 419.50 / 1217.70 / 1432.40 | 2666.67 | FAIL | PASS |
| Crossover | CINEMA | topology-disable | 512 | 3846.25 / 8901.10 / 10535.70 | 433.15 / 1215.20 / 1298.10 | 2666.67 | FAIL | PASS |
| AudioGraph | CINEMA | topology-disable | 512 | 3788.30 / 10548.20 / 11160.60 | 498.25 / 1220.20 / 1318.70 | 2666.67 | FAIL | PASS |
| Crossover | CINEMA | IIR-FIR-transition | 512 | 2437.20 / 4884.40 / 6273.70 | 1047.80 / 2035.30 / 2291.70 | 2666.67 | FAIL | PASS |
| AudioGraph | CINEMA | IIR-FIR-transition | 512 | 2593.85 / 4609.80 / 4931.60 | 1133.70 / 2116.30 / 2204.40 | 2666.67 | FAIL | PASS |
| Crossover | CINEMA | FIR-length-transition | 512 | 2744.65 / 4228.60 / 4848.10 | 2048.70 / 3803.50 / 4196.10 | 2666.67 | FAIL | FAIL |
| AudioGraph | CINEMA | FIR-length-transition | 512 | 2830.20 / 4336.70 / 4980.60 | 2136.15 / 3401.10 / 4513.70 | 2666.67 | FAIL | FAIL |
| Crossover | CINEMA | program-transition | 512 | 4072.65 / 6558.40 / 6986.90 | 2993.50 / 4703.30 / 8037.80 | 2666.67 | FAIL | FAIL |
| AudioGraph | CINEMA | program-transition | 512 | 4080.70 / 6409.40 / 7618.10 | 3140.40 / 4978.90 / 5263.90 | 2666.67 | FAIL | FAIL |
| Crossover | CINEMA | unity | 1024 | 437.20 / 701.70 / 2815.60 | 443.00 / 699.50 / 2199.00 | 5333.33 | PASS | PASS |
| OutputMatrix | CINEMA | unity | 1024 | 30.10 / 78.20 / 380.60 | 23.30 / 60.00 / 322.30 | 5333.33 | PASS | PASS |
| Headroom | CINEMA | unity | 1024 | 29.70 / 88.70 / 728.30 | 30.50 / 94.40 / 4674.50 | 5333.33 | PASS | PASS |
| AudioGraph | CINEMA | unity | 1024 | 523.30 / 814.50 / 2456.20 | 525.20 / 868.40 / 5859.60 | 5333.33 | PASS | FAIL |
| Crossover | CINEMA | IIR-AP | 1024 | 632.10 / 946.90 / 3048.50 | 633.20 / 966.90 / 8123.50 | 5333.33 | PASS | FAIL |
| OutputMatrix | CINEMA | IIR-AP | 1024 | 30.20 / 81.80 / 635.40 | 23.30 / 65.60 / 232.90 | 5333.33 | PASS | PASS |
| Headroom | CINEMA | IIR-AP | 1024 | 29.60 / 72.60 / 587.50 | 29.50 / 77.70 / 276.90 | 5333.33 | PASS | PASS |
| AudioGraph | CINEMA | IIR-AP | 1024 | 722.50 / 1102.70 / 3574.10 | 714.10 / 1132.30 / 2213.70 | 5333.33 | PASS | PASS |
| Crossover | CINEMA | FIR8193 | 1024 | 2917.85 / 4750.70 / 22391.40 | 2172.65 / 3670.70 / 30333.50 | 5333.33 | FAIL | FAIL |
| OutputMatrix | CINEMA | FIR8193 | 1024 | 30.00 / 78.60 / 545.80 | 23.30 / 64.30 / 305.70 | 5333.33 | PASS | PASS |
| Headroom | CINEMA | FIR8193 | 1024 | 29.90 / 85.30 / 353.10 | 29.50 / 79.20 / 701.00 | 5333.33 | PASS | PASS |
| AudioGraph | CINEMA | FIR8193 | 1024 | 3023.10 / 4773.90 / 7235.90 | 2302.90 / 3914.50 / 41022.30 | 5333.33 | FAIL | FAIL |
| Crossover | CINEMA | FIR8193-AP | 1024 | 2939.50 / 4793.30 / 46018.10 | 2181.50 / 3565.50 / 5185.40 | 5333.33 | FAIL | PASS |
| OutputMatrix | CINEMA | FIR8193-AP | 1024 | 30.20 / 76.80 / 555.30 | 23.40 / 68.90 / 741.30 | 5333.33 | PASS | PASS |
| Headroom | CINEMA | FIR8193-AP | 1024 | 29.40 / 110.60 / 385.50 | 30.40 / 77.90 / 3764.90 | 5333.33 | PASS | PASS |
| AudioGraph | CINEMA | FIR8193-AP | 1024 | 3064.05 / 4808.30 / 6878.40 | 2307.85 / 3823.00 / 5658.80 | 5333.33 | FAIL | FAIL |
| Crossover | CINEMA | filter-transition | 1024 | 5966.90 / 8065.80 / 9821.30 | 4359.80 / 6464.70 / 7570.80 | 5333.33 | FAIL | FAIL |
| AudioGraph | CINEMA | filter-transition | 1024 | 6287.30 / 8111.70 / 8495.50 | 4551.00 / 6358.40 / 6787.20 | 5333.33 | FAIL | FAIL |
| Crossover | CINEMA | emergency-mute | 1024 | 4296.25 / 9723.60 / 11034.60 | 851.65 / 1779.90 / 1899.50 | 5333.33 | FAIL | PASS |
| AudioGraph | CINEMA | emergency-mute | 1024 | 4529.95 / 9732.90 / 10284.60 | 1119.25 / 1712.80 / 2123.80 | 5333.33 | FAIL | PASS |
| Crossover | CINEMA | topology-disable | 1024 | 4529.60 / 8552.60 / 9372.00 | 814.25 / 2155.40 / 2301.00 | 5333.33 | FAIL | PASS |
| AudioGraph | CINEMA | topology-disable | 1024 | 4679.30 / 10015.00 / 11234.40 | 967.65 / 2404.50 / 2604.60 | 5333.33 | FAIL | PASS |
| Crossover | CINEMA | IIR-FIR-transition | 1024 | 3859.85 / 6043.00 / 6110.80 | 2055.75 / 3677.40 / 11040.40 | 5333.33 | FAIL | FAIL |
| AudioGraph | CINEMA | IIR-FIR-transition | 1024 | 3874.90 / 6122.00 / 7221.20 | 2203.85 / 3473.70 / 4030.40 | 5333.33 | FAIL | PASS |
| Crossover | CINEMA | FIR-length-transition | 1024 | 4406.25 / 6283.20 / 6704.90 | 3238.40 / 4836.10 / 5549.50 | 5333.33 | FAIL | FAIL |
| AudioGraph | CINEMA | FIR-length-transition | 1024 | 4391.60 / 6383.50 / 8433.60 | 3422.60 / 5219.00 / 7274.50 | 5333.33 | FAIL | FAIL |
| Crossover | CINEMA | program-transition | 1024 | 6178.00 / 9383.50 / 10519.20 | 4651.55 / 7167.50 / 7671.10 | 5333.33 | FAIL | FAIL |
| AudioGraph | CINEMA | program-transition | 1024 | 6385.65 / 8386.10 / 9202.00 | 4759.05 / 6473.60 / 9019.10 | 5333.33 | FAIL | FAIL |

## Cicluri CPU ale thread-ului

Statisticile de cicluri sunt sortate separat. Maximum în cicluri nu este obligatoriu același callback cu maximum QPC. Perechile brute și `thread_cycles_at_max_wall` rămân în arhiva locală; overhead-ul API este raportat în logurile fiecărei execuții.

| Modul | Program | Configurație | Frames | Înainte median/p99/max cycles | După median/p99/max cycles | Ratio median după/înainte |
| --- | --- | --- | --- | --- | --- | --- |
| Crossover | MUSIC | unity | 128 | 155700.00 / 318000.00 / 414000.00 | 159000.00 / 327000.00 / 517800.00 | 1.0212 |
| OutputMatrix | MUSIC | unity | 128 | 12900.00 / 61800.00 / 108900.00 | 9600.00 / 52200.00 / 106200.00 | 0.7442 |
| Headroom | MUSIC | unity | 128 | 12300.00 / 64800.00 / 108000.00 | 12600.00 / 58800.00 / 116700.00 | 1.0244 |
| AudioGraph | MUSIC | unity | 128 | 188700.00 / 373800.00 / 577200.00 | 188700.00 / 366600.00 / 531000.00 | 1.0000 |
| Crossover | MUSIC | IIR-AP | 128 | 227100.00 / 415800.00 / 711300.00 | 226800.00 / 426300.00 / 570000.00 | 0.9987 |
| OutputMatrix | MUSIC | IIR-AP | 128 | 12900.00 / 61500.00 / 105900.00 | 9600.00 / 57600.00 / 115800.00 | 0.7442 |
| Headroom | MUSIC | IIR-AP | 128 | 12600.00 / 60900.00 / 109200.00 | 12600.00 / 63900.00 / 119400.00 | 1.0000 |
| AudioGraph | MUSIC | IIR-AP | 128 | 259500.00 / 461100.00 / 692100.00 | 254700.00 / 461700.00 / 621000.00 | 0.9815 |
| Crossover | MUSIC | FIR8193 | 128 | 1758900.00 / 4725000.00 / 9431100.00 | 1077300.00 / 3132600.00 / 5605200.00 | 0.6125 |
| OutputMatrix | MUSIC | FIR8193 | 128 | 12900.00 / 60600.00 / 113700.00 | 9600.00 / 55500.00 / 105000.00 | 0.7442 |
| Headroom | MUSIC | FIR8193 | 128 | 12600.00 / 58200.00 / 150600.00 | 12600.00 / 68100.00 / 107700.00 | 1.0000 |
| AudioGraph | MUSIC | FIR8193 | 128 | 1753350.00 / 4678200.00 / 7562700.00 | 1130400.00 / 3182700.00 / 5139300.00 | 0.6447 |
| Crossover | MUSIC | FIR8193-AP | 128 | 1740300.00 / 4491000.00 / 11689200.00 | 1100850.00 / 3136500.00 / 9988200.00 | 0.6326 |
| OutputMatrix | MUSIC | FIR8193-AP | 128 | 12900.00 / 58200.00 / 109800.00 | 9600.00 / 54000.00 / 103500.00 | 0.7442 |
| Headroom | MUSIC | FIR8193-AP | 128 | 12600.00 / 59700.00 / 165300.00 | 12600.00 / 60600.00 / 1665300.00 | 1.0000 |
| AudioGraph | MUSIC | FIR8193-AP | 128 | 1898250.00 / 4885200.00 / 9438300.00 | 1136700.00 / 3144600.00 / 7974600.00 | 0.5988 |
| Crossover | MUSIC | filter-transition | 128 | 6777600.00 / 11668800.00 / 13767900.00 | 4883550.00 / 8305200.00 / 8940900.00 | 0.7205 |
| AudioGraph | MUSIC | filter-transition | 128 | 6663150.00 / 12379500.00 / 16380000.00 | 5062950.00 / 7960500.00 / 8998800.00 | 0.7598 |
| Crossover | MUSIC | emergency-mute | 128 | 9997800.00 / 27268500.00 / 28900200.00 | 421800.00 / 1025100.00 / 1089300.00 | 0.0422 |
| AudioGraph | MUSIC | emergency-mute | 128 | 8899500.00 / 26923800.00 / 31518300.00 | 469050.00 / 1178400.00 / 1207500.00 | 0.0527 |
| Crossover | MUSIC | topology-disable | 128 | 8790600.00 / 26363700.00 / 31998900.00 | 435750.00 / 1263300.00 / 1392900.00 | 0.0496 |
| AudioGraph | MUSIC | topology-disable | 128 | 9196350.00 / 25861800.00 / 30638400.00 | 506700.00 / 1464300.00 / 1660200.00 | 0.0551 |
| Crossover | MUSIC | IIR-FIR-transition | 128 | 4382550.00 / 8954100.00 / 9551700.00 | 964650.00 / 2716500.00 / 3126300.00 | 0.2201 |
| AudioGraph | MUSIC | IIR-FIR-transition | 128 | 4570050.00 / 10738800.00 / 11927400.00 | 964950.00 / 2355000.00 / 3095700.00 | 0.2111 |
| Crossover | MUSIC | FIR-length-transition | 128 | 4602300.00 / 8875500.00 / 9249900.00 | 3243900.00 / 5972100.00 / 6484200.00 | 0.7048 |
| AudioGraph | MUSIC | FIR-length-transition | 128 | 4756050.00 / 10051500.00 / 10748700.00 | 3394050.00 / 6291600.00 / 7281900.00 | 0.7136 |
| Crossover | MUSIC | program-transition | 128 | 7019400.00 / 11666700.00 / 12411900.00 | 4749750.00 / 9003300.00 / 10745400.00 | 0.6767 |
| AudioGraph | MUSIC | program-transition | 128 | 7203750.00 / 12016200.00 / 13860300.00 | 5066100.00 / 9092700.00 / 9742800.00 | 0.7033 |
| Crossover | MUSIC | unity | 256 | 303300.00 / 521400.00 / 806400.00 | 309300.00 / 535800.00 / 712800.00 | 1.0198 |
| OutputMatrix | MUSIC | unity | 256 | 24000.00 / 87900.00 / 127500.00 | 17700.00 / 70500.00 / 124800.00 | 0.7375 |
| Headroom | MUSIC | unity | 256 | 23700.00 / 84300.00 / 127200.00 | 23700.00 / 85200.00 / 120000.00 | 1.0000 |
| AudioGraph | MUSIC | unity | 256 | 364800.00 / 614100.00 / 850200.00 | 364200.00 / 623400.00 / 1294200.00 | 0.9984 |
| Crossover | MUSIC | IIR-AP | 256 | 444600.00 / 723000.00 / 972900.00 | 445200.00 / 717900.00 / 950100.00 | 1.0013 |
| OutputMatrix | MUSIC | IIR-AP | 256 | 24300.00 / 87000.00 / 119400.00 | 17700.00 / 73800.00 / 123000.00 | 0.7284 |
| Headroom | MUSIC | IIR-AP | 256 | 23400.00 / 81000.00 / 125700.00 | 23400.00 / 87300.00 / 160200.00 | 1.0000 |
| AudioGraph | MUSIC | IIR-AP | 256 | 510600.00 / 825000.00 / 5706600.00 | 500400.00 / 847500.00 / 2466300.00 | 0.9800 |
| Crossover | MUSIC | FIR8193 | 256 | 2626500.00 / 6012000.00 / 10918800.00 | 1839900.00 / 4156500.00 / 7839300.00 | 0.7005 |
| OutputMatrix | MUSIC | FIR8193 | 256 | 24300.00 / 87600.00 / 200100.00 | 17700.00 / 73800.00 / 1718100.00 | 0.7284 |
| Headroom | MUSIC | FIR8193 | 256 | 23700.00 / 84900.00 / 146100.00 | 23700.00 / 84300.00 / 116400.00 | 1.0000 |
| AudioGraph | MUSIC | FIR8193 | 256 | 2850450.00 / 6322800.00 / 10665900.00 | 2019000.00 / 4762800.00 / 9007200.00 | 0.7083 |
| Crossover | MUSIC | FIR8193-AP | 256 | 2749350.00 / 6370200.00 / 8908500.00 | 2171850.00 / 4930800.00 / 6845100.00 | 0.7900 |
| OutputMatrix | MUSIC | FIR8193-AP | 256 | 24000.00 / 91200.00 / 1706700.00 | 17700.00 / 69300.00 / 111000.00 | 0.7375 |
| Headroom | MUSIC | FIR8193-AP | 256 | 23400.00 / 88800.00 / 146100.00 | 24300.00 / 84900.00 / 139800.00 | 1.0385 |
| AudioGraph | MUSIC | FIR8193-AP | 256 | 2792550.00 / 6256500.00 / 9476400.00 | 2304150.00 / 5147100.00 / 7241400.00 | 0.8251 |
| Crossover | MUSIC | filter-transition | 256 | 8244900.00 / 13620600.00 / 15143400.00 | 5811900.00 / 9572100.00 / 10357200.00 | 0.7049 |
| AudioGraph | MUSIC | filter-transition | 256 | 8552400.00 / 12937200.00 / 17606100.00 | 5963100.00 / 10829400.00 / 14284200.00 | 0.6972 |
| Crossover | MUSIC | emergency-mute | 256 | 10509450.00 / 24967200.00 / 30798900.00 | 623400.00 / 1758000.00 / 1783500.00 | 0.0593 |
| AudioGraph | MUSIC | emergency-mute | 256 | 10109850.00 / 24324300.00 / 27251700.00 | 742200.00 / 1575900.00 / 1648500.00 | 0.0734 |
| Crossover | MUSIC | topology-disable | 256 | 10433700.00 / 28154100.00 / 28602900.00 | 705750.00 / 1907700.00 / 2284800.00 | 0.0676 |
| AudioGraph | MUSIC | topology-disable | 256 | 10618200.00 / 22224300.00 / 25887300.00 | 810300.00 / 2101800.00 / 2277000.00 | 0.0763 |
| Crossover | MUSIC | IIR-FIR-transition | 256 | 5531850.00 / 10139100.00 / 11702400.00 | 1692000.00 / 3240300.00 / 4011000.00 | 0.3059 |
| AudioGraph | MUSIC | IIR-FIR-transition | 256 | 5400450.00 / 10890300.00 / 12518100.00 | 1796850.00 / 4018500.00 / 4599300.00 | 0.3327 |
| Crossover | MUSIC | FIR-length-transition | 256 | 6096900.00 / 10178100.00 / 10643400.00 | 4192050.00 / 7398000.00 / 8172900.00 | 0.6876 |
| AudioGraph | MUSIC | FIR-length-transition | 256 | 5804250.00 / 11242500.00 / 11571300.00 | 4294050.00 / 7211100.00 / 7803600.00 | 0.7398 |
| Crossover | MUSIC | program-transition | 256 | 8636400.00 / 13553100.00 / 13996800.00 | 6333900.00 / 10751400.00 / 11865600.00 | 0.7334 |
| AudioGraph | MUSIC | program-transition | 256 | 8773200.00 / 13170900.00 / 14565300.00 | 6167550.00 / 11139600.00 / 12390900.00 | 0.7030 |
| Crossover | MUSIC | unity | 512 | 621900.00 / 974100.00 / 1244700.00 | 613500.00 / 983700.00 / 1343700.00 | 0.9865 |
| OutputMatrix | MUSIC | unity | 512 | 45900.00 / 129900.00 / 190500.00 | 34200.00 / 106500.00 / 235200.00 | 0.7451 |
| Headroom | MUSIC | unity | 512 | 45900.00 / 129300.00 / 342000.00 | 45300.00 / 131700.00 / 1680600.00 | 0.9869 |
| AudioGraph | MUSIC | unity | 512 | 736800.00 / 1151700.00 / 1443300.00 | 726000.00 / 1129800.00 / 5852400.00 | 0.9853 |
| Crossover | MUSIC | IIR-AP | 512 | 901950.00 / 1348800.00 / 1881600.00 | 919500.00 / 1366200.00 / 7883700.00 | 1.0195 |
| OutputMatrix | MUSIC | IIR-AP | 512 | 46200.00 / 129900.00 / 188400.00 | 34500.00 / 107700.00 / 163200.00 | 0.7468 |
| Headroom | MUSIC | IIR-AP | 512 | 45600.00 / 130500.00 / 196500.00 | 46200.00 / 128700.00 / 182700.00 | 1.0132 |
| AudioGraph | MUSIC | IIR-AP | 512 | 1052100.00 / 1527600.00 / 2832300.00 | 1072500.00 / 1530900.00 / 6108000.00 | 1.0194 |
| Crossover | MUSIC | FIR8193 | 512 | 4761450.00 / 8734800.00 / 12647700.00 | 3416700.00 / 6490800.00 / 11060700.00 | 0.7176 |
| OutputMatrix | MUSIC | FIR8193 | 512 | 47700.00 / 134400.00 / 180900.00 | 34500.00 / 108600.00 / 172800.00 | 0.7233 |
| Headroom | MUSIC | FIR8193 | 512 | 45600.00 / 126000.00 / 217200.00 | 45300.00 / 125400.00 / 181800.00 | 0.9934 |
| AudioGraph | MUSIC | FIR8193 | 512 | 4993350.00 / 9014700.00 / 11921100.00 | 3612450.00 / 6563100.00 / 12702900.00 | 0.7235 |
| Crossover | MUSIC | FIR8193-AP | 512 | 4750800.00 / 8746500.00 / 11733900.00 | 3446100.00 / 6419100.00 / 10635900.00 | 0.7254 |
| OutputMatrix | MUSIC | FIR8193-AP | 512 | 45900.00 / 129600.00 / 195900.00 | 34500.00 / 107100.00 / 1726800.00 | 0.7516 |
| Headroom | MUSIC | FIR8193-AP | 512 | 45600.00 / 133800.00 / 211800.00 | 45600.00 / 127500.00 / 208500.00 | 1.0000 |
| AudioGraph | MUSIC | FIR8193-AP | 512 | 5001450.00 / 9213000.00 / 13325400.00 | 3649050.00 / 6896100.00 / 11140500.00 | 0.7296 |
| Crossover | MUSIC | filter-transition | 512 | 11805000.00 / 16197600.00 / 18574800.00 | 8385600.00 / 13479300.00 / 14038800.00 | 0.7103 |
| AudioGraph | MUSIC | filter-transition | 512 | 12196650.00 / 17565900.00 / 17674800.00 | 8546850.00 / 12850200.00 / 14060400.00 | 0.7008 |
| Crossover | MUSIC | emergency-mute | 512 | 11100300.00 / 24803700.00 / 29383200.00 | 988200.00 / 2400600.00 / 2542800.00 | 0.0890 |
| AudioGraph | MUSIC | emergency-mute | 512 | 11069550.00 / 26247300.00 / 28862400.00 | 1213800.00 / 2873700.00 / 3067200.00 | 0.1097 |
| Crossover | MUSIC | topology-disable | 512 | 10688400.00 / 22831500.00 / 31194300.00 | 1254000.00 / 3272100.00 / 3852900.00 | 0.1173 |
| AudioGraph | MUSIC | topology-disable | 512 | 10937850.00 / 26276400.00 / 27577200.00 | 1422600.00 / 3699000.00 / 3838500.00 | 0.1301 |
| Crossover | MUSIC | IIR-FIR-transition | 512 | 7441500.00 / 12772500.00 / 14127000.00 | 3246600.00 / 6687000.00 / 7536900.00 | 0.4363 |
| AudioGraph | MUSIC | IIR-FIR-transition | 512 | 7390950.00 / 13124400.00 / 14210400.00 | 3360000.00 / 7213800.00 / 8348100.00 | 0.4546 |
| Crossover | MUSIC | FIR-length-transition | 512 | 7893900.00 / 12098100.00 / 13274100.00 | 6389400.00 / 9411600.00 / 10391400.00 | 0.8094 |
| AudioGraph | MUSIC | FIR-length-transition | 512 | 8104350.00 / 12490200.00 / 14830500.00 | 6232050.00 / 8825700.00 / 10467300.00 | 0.7690 |
| Crossover | MUSIC | program-transition | 512 | 11917800.00 / 17198700.00 / 19069800.00 | 8656950.00 / 12717300.00 / 14431200.00 | 0.7264 |
| AudioGraph | MUSIC | program-transition | 512 | 12203400.00 / 16503600.00 / 16731000.00 | 8951400.00 / 13848000.00 / 14914800.00 | 0.7335 |
| Crossover | MUSIC | unity | 1024 | 1297500.00 / 1745100.00 / 2244300.00 | 1370100.00 / 1917600.00 / 2343000.00 | 1.0560 |
| OutputMatrix | MUSIC | unity | 1024 | 92100.00 / 209400.00 / 593700.00 | 71400.00 / 168900.00 / 241800.00 | 0.7752 |
| Headroom | MUSIC | unity | 1024 | 90300.00 / 200100.00 / 288600.00 | 90600.00 / 207000.00 / 299700.00 | 1.0033 |
| AudioGraph | MUSIC | unity | 1024 | 1552800.00 / 2137800.00 / 2756400.00 | 1555200.00 / 2194200.00 / 3288000.00 | 1.0015 |
| Crossover | MUSIC | IIR-AP | 1024 | 1881300.00 / 2527800.00 / 9047400.00 | 1896300.00 / 2509500.00 / 3192900.00 | 1.0080 |
| OutputMatrix | MUSIC | IIR-AP | 1024 | 91800.00 / 212100.00 / 384600.00 | 71100.00 / 170700.00 / 285300.00 | 0.7745 |
| Headroom | MUSIC | IIR-AP | 1024 | 92400.00 / 212100.00 / 1544700.00 | 92100.00 / 208500.00 / 320700.00 | 0.9968 |
| AudioGraph | MUSIC | IIR-AP | 1024 | 2190000.00 / 2951100.00 / 3631500.00 | 2119500.00 / 2874600.00 / 3927000.00 | 0.9678 |
| Crossover | MUSIC | FIR8193 | 1024 | 8470500.00 / 12846600.00 / 19213800.00 | 6397650.00 / 9790500.00 / 16611900.00 | 0.7553 |
| OutputMatrix | MUSIC | FIR8193 | 1024 | 91800.00 / 197100.00 / 268200.00 | 71400.00 / 174000.00 / 285600.00 | 0.7778 |
| Headroom | MUSIC | FIR8193 | 1024 | 90300.00 / 206100.00 / 1831500.00 | 90000.00 / 202500.00 / 290700.00 | 0.9967 |
| AudioGraph | MUSIC | FIR8193 | 1024 | 8902650.00 / 13506900.00 / 17261100.00 | 6855450.00 / 10535400.00 / 16236000.00 | 0.7700 |
| Crossover | MUSIC | FIR8193-AP | 1024 | 8639550.00 / 12993300.00 / 16207800.00 | 6479400.00 / 10064100.00 / 15690300.00 | 0.7500 |
| OutputMatrix | MUSIC | FIR8193-AP | 1024 | 91200.00 / 204300.00 / 324000.00 | 71400.00 / 167100.00 / 1750500.00 | 0.7829 |
| Headroom | MUSIC | FIR8193-AP | 1024 | 89700.00 / 199200.00 / 1777200.00 | 90000.00 / 208800.00 / 1844700.00 | 1.0033 |
| AudioGraph | MUSIC | FIR8193-AP | 1024 | 8971650.00 / 13632300.00 / 20393100.00 | 6846600.00 / 10388400.00 / 16687200.00 | 0.7631 |
| Crossover | MUSIC | filter-transition | 1024 | 17376900.00 / 22329300.00 / 22800000.00 | 12933000.00 / 17411400.00 / 22114500.00 | 0.7443 |
| AudioGraph | MUSIC | filter-transition | 1024 | 17946000.00 / 23626500.00 / 24871200.00 | 13369800.00 / 18214500.00 / 19258200.00 | 0.7450 |
| Crossover | MUSIC | emergency-mute | 1024 | 12812850.00 / 25747500.00 / 27138000.00 | 1867650.00 / 4852200.00 / 5017500.00 | 0.1458 |
| AudioGraph | MUSIC | emergency-mute | 1024 | 13278000.00 / 29193300.00 / 29799300.00 | 2169600.00 / 4935000.00 / 5208300.00 | 0.1634 |
| Crossover | MUSIC | topology-disable | 1024 | 12914700.00 / 27887700.00 / 31318200.00 | 2359200.00 / 5826600.00 / 5948700.00 | 0.1827 |
| AudioGraph | MUSIC | topology-disable | 1024 | 13329000.00 / 27928500.00 / 31782000.00 | 2720850.00 / 6267000.00 / 6577200.00 | 0.2041 |
| Crossover | MUSIC | IIR-FIR-transition | 1024 | 11232600.00 / 16302300.00 / 18255900.00 | 5848950.00 / 9970500.00 / 10197000.00 | 0.5207 |
| AudioGraph | MUSIC | IIR-FIR-transition | 1024 | 11687400.00 / 17567100.00 / 19157100.00 | 6635250.00 / 11277900.00 / 11482500.00 | 0.5677 |
| Crossover | MUSIC | FIR-length-transition | 1024 | 12841950.00 / 17159700.00 / 17508600.00 | 9647400.00 / 13822200.00 / 14996700.00 | 0.7512 |
| AudioGraph | MUSIC | FIR-length-transition | 1024 | 13257750.00 / 19772700.00 / 20182800.00 | 9871200.00 / 12384600.00 / 13123500.00 | 0.7446 |
| Crossover | MUSIC | program-transition | 1024 | 18303900.00 / 24134400.00 / 24541500.00 | 13705650.00 / 17847900.00 / 18247800.00 | 0.7488 |
| AudioGraph | MUSIC | program-transition | 1024 | 18742050.00 / 23635500.00 / 26128200.00 | 13902900.00 / 18512700.00 / 21848400.00 | 0.7418 |
| Crossover | CINEMA | unity | 128 | 156900.00 / 314100.00 / 522900.00 | 159600.00 / 326700.00 / 456000.00 | 1.0172 |
| OutputMatrix | CINEMA | unity | 128 | 12900.00 / 58800.00 / 130200.00 | 9600.00 / 62100.00 / 98100.00 | 0.7442 |
| Headroom | CINEMA | unity | 128 | 12600.00 / 60000.00 / 114600.00 | 12600.00 / 62700.00 / 130500.00 | 1.0000 |
| AudioGraph | CINEMA | unity | 128 | 188400.00 / 364800.00 / 537600.00 | 186600.00 / 360900.00 / 520800.00 | 0.9904 |
| Crossover | CINEMA | IIR-AP | 128 | 227400.00 / 411900.00 / 564000.00 | 227100.00 / 411600.00 / 804600.00 | 0.9987 |
| OutputMatrix | CINEMA | IIR-AP | 128 | 12900.00 / 55500.00 / 278400.00 | 9600.00 / 57600.00 / 110100.00 | 0.7442 |
| Headroom | CINEMA | IIR-AP | 128 | 12600.00 / 57600.00 / 116100.00 | 12600.00 / 62400.00 / 108600.00 | 1.0000 |
| AudioGraph | CINEMA | IIR-AP | 128 | 259500.00 / 460500.00 / 690300.00 | 256200.00 / 467700.00 / 768300.00 | 0.9873 |
| Crossover | CINEMA | FIR8193 | 128 | 1580850.00 / 4321800.00 / 6970800.00 | 1135800.00 / 3363000.00 / 6635400.00 | 0.7185 |
| OutputMatrix | CINEMA | FIR8193 | 128 | 12900.00 / 62400.00 / 113100.00 | 9600.00 / 56400.00 / 106200.00 | 0.7442 |
| Headroom | CINEMA | FIR8193 | 128 | 12600.00 / 60600.00 / 115200.00 | 12600.00 / 62400.00 / 124500.00 | 1.0000 |
| AudioGraph | CINEMA | FIR8193 | 128 | 1770450.00 / 4784400.00 / 7525200.00 | 1125150.00 / 3180600.00 / 6090600.00 | 0.6355 |
| Crossover | CINEMA | FIR8193-AP | 128 | 1972050.00 / 4918500.00 / 7914000.00 | 1054950.00 / 3040200.00 / 8983500.00 | 0.5350 |
| OutputMatrix | CINEMA | FIR8193-AP | 128 | 13200.00 / 62400.00 / 110100.00 | 9600.00 / 28800.00 / 98700.00 | 0.7273 |
| Headroom | CINEMA | FIR8193-AP | 128 | 13200.00 / 58800.00 / 92400.00 | 12600.00 / 62400.00 / 111900.00 | 0.9545 |
| AudioGraph | CINEMA | FIR8193-AP | 128 | 2449650.00 / 5783400.00 / 8133300.00 | 1134000.00 / 3228300.00 / 9355800.00 | 0.4629 |
| Crossover | CINEMA | filter-transition | 128 | 7523250.00 / 11388900.00 / 13846800.00 | 4772100.00 / 9342300.00 / 11298000.00 | 0.6343 |
| AudioGraph | CINEMA | filter-transition | 128 | 7123500.00 / 12052200.00 / 16255500.00 | 5191500.00 / 8747700.00 / 9935100.00 | 0.7288 |
| Crossover | CINEMA | emergency-mute | 128 | 8627550.00 / 23736900.00 / 25256700.00 | 418350.00 / 1077600.00 / 4793100.00 | 0.0485 |
| AudioGraph | CINEMA | emergency-mute | 128 | 9696150.00 / 30115200.00 / 32942100.00 | 511800.00 / 1057500.00 / 1299600.00 | 0.0528 |
| Crossover | CINEMA | topology-disable | 128 | 8704650.00 / 25208700.00 / 27159300.00 | 429600.00 / 1367100.00 / 1648500.00 | 0.0494 |
| AudioGraph | CINEMA | topology-disable | 128 | 10036050.00 / 25229400.00 / 25941300.00 | 491700.00 / 1461900.00 / 1628100.00 | 0.0490 |
| Crossover | CINEMA | IIR-FIR-transition | 128 | 4299450.00 / 10082700.00 / 10650600.00 | 998700.00 / 3235200.00 / 3831600.00 | 0.2323 |
| AudioGraph | CINEMA | IIR-FIR-transition | 128 | 4591350.00 / 9477300.00 / 13112400.00 | 1035450.00 / 2926200.00 / 3512700.00 | 0.2255 |
| Crossover | CINEMA | FIR-length-transition | 128 | 4567200.00 / 9053100.00 / 11386500.00 | 3149100.00 / 6015600.00 / 6832200.00 | 0.6895 |
| AudioGraph | CINEMA | FIR-length-transition | 128 | 4591950.00 / 8333100.00 / 9210000.00 | 3214800.00 / 6367500.00 / 6579900.00 | 0.7001 |
| Crossover | CINEMA | program-transition | 128 | 7218750.00 / 12032400.00 / 13809600.00 | 4983450.00 / 8683500.00 / 9337200.00 | 0.6903 |
| AudioGraph | CINEMA | program-transition | 128 | 7401300.00 / 11683800.00 / 13457700.00 | 5025600.00 / 9033300.00 / 9761700.00 | 0.6790 |
| Crossover | CINEMA | unity | 256 | 305400.00 / 545100.00 / 884100.00 | 308700.00 / 529200.00 / 772200.00 | 1.0108 |
| OutputMatrix | CINEMA | unity | 256 | 24000.00 / 82500.00 / 138300.00 | 17700.00 / 71700.00 / 107100.00 | 0.7375 |
| Headroom | CINEMA | unity | 256 | 23400.00 / 85200.00 / 140700.00 | 23400.00 / 87000.00 / 134400.00 | 1.0000 |
| AudioGraph | CINEMA | unity | 256 | 366900.00 / 622800.00 / 967200.00 | 363900.00 / 633000.00 / 839400.00 | 0.9918 |
| Crossover | CINEMA | IIR-AP | 256 | 446700.00 / 740700.00 / 1139100.00 | 445500.00 / 730800.00 / 1003500.00 | 0.9973 |
| OutputMatrix | CINEMA | IIR-AP | 256 | 24000.00 / 87300.00 / 121500.00 | 17700.00 / 71100.00 / 129000.00 | 0.7375 |
| Headroom | CINEMA | IIR-AP | 256 | 23400.00 / 83400.00 / 260400.00 | 23400.00 / 83100.00 / 158100.00 | 1.0000 |
| AudioGraph | CINEMA | IIR-AP | 256 | 512400.00 / 822000.00 / 1247100.00 | 499200.00 / 814200.00 / 5677200.00 | 0.9742 |
| Crossover | CINEMA | FIR8193 | 256 | 2614650.00 / 6174900.00 / 12057300.00 | 1852200.00 / 4359900.00 / 8121600.00 | 0.7084 |
| OutputMatrix | CINEMA | FIR8193 | 256 | 24300.00 / 85500.00 / 1677900.00 | 17700.00 / 68700.00 / 166200.00 | 0.7284 |
| Headroom | CINEMA | FIR8193 | 256 | 23700.00 / 85500.00 / 118500.00 | 23400.00 / 88200.00 / 132600.00 | 0.9873 |
| AudioGraph | CINEMA | FIR8193 | 256 | 2809500.00 / 6436200.00 / 9492000.00 | 1945800.00 / 4396800.00 / 11740500.00 | 0.6926 |
| Crossover | CINEMA | FIR8193-AP | 256 | 2749650.00 / 5984100.00 / 9893400.00 | 1918500.00 / 4212000.00 / 8202600.00 | 0.6977 |
| OutputMatrix | CINEMA | FIR8193-AP | 256 | 24000.00 / 83100.00 / 145800.00 | 17700.00 / 71100.00 / 120300.00 | 0.7375 |
| Headroom | CINEMA | FIR8193-AP | 256 | 23400.00 / 93600.00 / 151500.00 | 23700.00 / 84600.00 / 130200.00 | 1.0128 |
| AudioGraph | CINEMA | FIR8193-AP | 256 | 2841900.00 / 6351900.00 / 14634300.00 | 2002500.00 / 4463700.00 / 7106400.00 | 0.7046 |
| Crossover | CINEMA | filter-transition | 256 | 8577450.00 / 14349000.00 / 15349200.00 | 6234150.00 / 9968100.00 / 10896900.00 | 0.7268 |
| AudioGraph | CINEMA | filter-transition | 256 | 8610750.00 / 13129500.00 / 14265900.00 | 6068700.00 / 11539200.00 / 16007700.00 | 0.7048 |
| Crossover | CINEMA | emergency-mute | 256 | 9172950.00 / 25297800.00 / 26201400.00 | 652050.00 / 1336500.00 / 1584000.00 | 0.0711 |
| AudioGraph | CINEMA | emergency-mute | 256 | 9390450.00 / 26004900.00 / 32686200.00 | 728850.00 / 1904700.00 / 2077500.00 | 0.0776 |
| Crossover | CINEMA | topology-disable | 256 | 9829050.00 / 25369500.00 / 30710100.00 | 696000.00 / 1942200.00 / 2137500.00 | 0.0708 |
| AudioGraph | CINEMA | topology-disable | 256 | 9915450.00 / 21603600.00 / 24814800.00 | 776250.00 / 2225700.00 / 2417100.00 | 0.0783 |
| Crossover | CINEMA | IIR-FIR-transition | 256 | 5207100.00 / 11799000.00 / 13755600.00 | 1658250.00 / 4749900.00 / 4972500.00 | 0.3185 |
| AudioGraph | CINEMA | IIR-FIR-transition | 256 | 5352600.00 / 10027800.00 / 10656300.00 | 1826850.00 / 3995700.00 / 4760100.00 | 0.3413 |
| Crossover | CINEMA | FIR-length-transition | 256 | 5779800.00 / 10293300.00 / 11676000.00 | 4167900.00 / 7443300.00 / 7576800.00 | 0.7211 |
| AudioGraph | CINEMA | FIR-length-transition | 256 | 5829600.00 / 9696600.00 / 11686200.00 | 4397250.00 / 7583400.00 / 7952400.00 | 0.7543 |
| Crossover | CINEMA | program-transition | 256 | 9171900.00 / 12828600.00 / 14477100.00 | 6400050.00 / 9664200.00 / 10089900.00 | 0.6978 |
| AudioGraph | CINEMA | program-transition | 256 | 8753250.00 / 14361600.00 / 16236300.00 | 6266400.00 / 10406400.00 / 12342300.00 | 0.7159 |
| Crossover | CINEMA | unity | 512 | 604800.00 / 961800.00 / 5447100.00 | 612000.00 / 952800.00 / 1305600.00 | 1.0119 |
| OutputMatrix | CINEMA | unity | 512 | 46200.00 / 131400.00 / 202500.00 | 34500.00 / 104100.00 / 168000.00 | 0.7468 |
| Headroom | CINEMA | unity | 512 | 45300.00 / 132600.00 / 1776000.00 | 46500.00 / 132300.00 / 195900.00 | 1.0265 |
| AudioGraph | CINEMA | unity | 512 | 732000.00 / 1153500.00 / 5792400.00 | 779700.00 / 1152300.00 / 1427700.00 | 1.0652 |
| Crossover | CINEMA | IIR-AP | 512 | 905100.00 / 1360200.00 / 2110500.00 | 951900.00 / 1359600.00 / 1975200.00 | 1.0517 |
| OutputMatrix | CINEMA | IIR-AP | 512 | 46200.00 / 133200.00 / 193800.00 | 36000.00 / 90600.00 / 146700.00 | 0.7792 |
| Headroom | CINEMA | IIR-AP | 512 | 45600.00 / 130800.00 / 189600.00 | 45600.00 / 120000.00 / 195900.00 | 1.0000 |
| AudioGraph | CINEMA | IIR-AP | 512 | 1073550.00 / 1533600.00 / 6087000.00 | 1087200.00 / 1538700.00 / 2147700.00 | 1.0127 |
| Crossover | CINEMA | FIR8193 | 512 | 4662000.00 / 8462700.00 / 13827000.00 | 3416250.00 / 6232500.00 / 9295800.00 | 0.7328 |
| OutputMatrix | CINEMA | FIR8193 | 512 | 45900.00 / 133200.00 / 191700.00 | 34200.00 / 106200.00 / 179100.00 | 0.7451 |
| Headroom | CINEMA | FIR8193 | 512 | 45300.00 / 129900.00 / 190800.00 | 45300.00 / 128400.00 / 227100.00 | 1.0000 |
| AudioGraph | CINEMA | FIR8193 | 512 | 4969500.00 / 9111900.00 / 12807000.00 | 3555000.00 / 6600000.00 / 10676700.00 | 0.7154 |
| Crossover | CINEMA | FIR8193-AP | 512 | 4755900.00 / 8644500.00 / 15173400.00 | 3421650.00 / 6189300.00 / 9730800.00 | 0.7195 |
| OutputMatrix | CINEMA | FIR8193-AP | 512 | 45900.00 / 128100.00 / 197700.00 | 34500.00 / 111000.00 / 174600.00 | 0.7516 |
| Headroom | CINEMA | FIR8193-AP | 512 | 45600.00 / 132000.00 / 222600.00 | 45600.00 / 127200.00 / 184500.00 | 1.0000 |
| AudioGraph | CINEMA | FIR8193-AP | 512 | 4906650.00 / 8819700.00 / 11434800.00 | 3678750.00 / 6870600.00 / 11976000.00 | 0.7497 |
| Crossover | CINEMA | filter-transition | 512 | 11557800.00 / 17625300.00 / 21031500.00 | 8292900.00 / 11734500.00 / 14025300.00 | 0.7175 |
| AudioGraph | CINEMA | filter-transition | 512 | 11929050.00 / 16942800.00 / 20973900.00 | 8598900.00 / 12705900.00 / 18592800.00 | 0.7208 |
| Crossover | CINEMA | emergency-mute | 512 | 10918500.00 / 27957300.00 / 29592900.00 | 1042650.00 / 2564100.00 / 2598600.00 | 0.0955 |
| AudioGraph | CINEMA | emergency-mute | 512 | 10736700.00 / 26522100.00 / 30617100.00 | 1264200.00 / 3034500.00 / 3471900.00 | 0.1177 |
| Crossover | CINEMA | topology-disable | 512 | 11478450.00 / 26051100.00 / 27249000.00 | 1269750.00 / 3207000.00 / 3387600.00 | 0.1106 |
| AudioGraph | CINEMA | topology-disable | 512 | 10929600.00 / 27964200.00 / 31069800.00 | 1372650.00 / 3409200.00 / 3484800.00 | 0.1256 |
| Crossover | CINEMA | IIR-FIR-transition | 512 | 7243650.00 / 13418400.00 / 14400300.00 | 3010650.00 / 5871000.00 / 6320400.00 | 0.4156 |
| AudioGraph | CINEMA | IIR-FIR-transition | 512 | 7527750.00 / 13468800.00 / 14469000.00 | 3335400.00 / 6151800.00 / 6606000.00 | 0.4431 |
| Crossover | CINEMA | FIR-length-transition | 512 | 8077950.00 / 11892900.00 / 12975900.00 | 6107550.00 / 9682800.00 / 10692000.00 | 0.7561 |
| AudioGraph | CINEMA | FIR-length-transition | 512 | 8202300.00 / 12466800.00 / 13297500.00 | 6282600.00 / 9576300.00 / 10469400.00 | 0.7660 |
| Crossover | CINEMA | program-transition | 512 | 11916000.00 / 18005400.00 / 18633000.00 | 8822250.00 / 12841800.00 / 13474200.00 | 0.7404 |
| AudioGraph | CINEMA | program-transition | 512 | 11898900.00 / 17757300.00 / 18336600.00 | 9359550.00 / 13248300.00 / 13500900.00 | 0.7866 |
| Crossover | CINEMA | unity | 1024 | 1305000.00 / 1788900.00 / 8292300.00 | 1326300.00 / 1845600.00 / 6566100.00 | 1.0163 |
| OutputMatrix | CINEMA | unity | 1024 | 91500.00 / 207000.00 / 1647300.00 | 71100.00 / 165000.00 / 238800.00 | 0.7770 |
| Headroom | CINEMA | unity | 1024 | 90300.00 / 204900.00 / 1758600.00 | 92700.00 / 203100.00 / 1314300.00 | 1.0266 |
| AudioGraph | CINEMA | unity | 1024 | 1559400.00 / 2161200.00 / 7366800.00 | 1564500.00 / 2202000.00 / 6736500.00 | 1.0033 |
| Crossover | CINEMA | IIR-AP | 1024 | 1885200.00 / 2529600.00 / 9132000.00 | 1887600.00 / 2579700.00 / 3084000.00 | 1.0013 |
| OutputMatrix | CINEMA | IIR-AP | 1024 | 91800.00 / 207600.00 / 322500.00 | 71100.00 / 171600.00 / 251400.00 | 0.7745 |
| Headroom | CINEMA | IIR-AP | 1024 | 90300.00 / 205200.00 / 1764000.00 | 90000.00 / 206100.00 / 307500.00 | 0.9967 |
| AudioGraph | CINEMA | IIR-AP | 1024 | 2142900.00 / 2916000.00 / 9851400.00 | 2129400.00 / 2951100.00 / 6350100.00 | 0.9937 |
| Crossover | CINEMA | FIR8193 | 1024 | 8517300.00 / 13005300.00 / 17844000.00 | 6409200.00 / 9897900.00 / 15968100.00 | 0.7525 |
| OutputMatrix | CINEMA | FIR8193 | 1024 | 91500.00 / 203400.00 / 282300.00 | 71100.00 / 167700.00 / 261000.00 | 0.7770 |
| Headroom | CINEMA | FIR8193 | 1024 | 91200.00 / 204600.00 / 298200.00 | 89700.00 / 202200.00 / 1644300.00 | 0.9836 |
| AudioGraph | CINEMA | FIR8193 | 1024 | 8833200.00 / 13331400.00 / 17713200.00 | 6788250.00 / 10486500.00 / 13567800.00 | 0.7685 |
| Crossover | CINEMA | FIR8193-AP | 1024 | 8582700.00 / 13073400.00 / 18743700.00 | 6445200.00 / 9800400.00 / 14856600.00 | 0.7510 |
| OutputMatrix | CINEMA | FIR8193-AP | 1024 | 92100.00 / 203700.00 / 1469100.00 | 71400.00 / 171900.00 / 575700.00 | 0.7752 |
| Headroom | CINEMA | FIR8193-AP | 1024 | 89700.00 / 205500.00 / 264000.00 | 92700.00 / 204000.00 / 1299300.00 | 1.0334 |
| AudioGraph | CINEMA | FIR8193-AP | 1024 | 8946900.00 / 13320600.00 / 17922900.00 | 6785100.00 / 10477500.00 / 16365000.00 | 0.7584 |
| Crossover | CINEMA | filter-transition | 1024 | 17362950.00 / 22106400.00 / 26926500.00 | 12901050.00 / 17954400.00 / 20968500.00 | 0.7430 |
| AudioGraph | CINEMA | filter-transition | 1024 | 18110250.00 / 22454400.00 / 23968800.00 | 13344000.00 / 17414400.00 / 18228300.00 | 0.7368 |
| Crossover | CINEMA | emergency-mute | 1024 | 12317700.00 / 27588600.00 / 30254700.00 | 1812450.00 / 4844400.00 / 4951200.00 | 0.1471 |
| AudioGraph | CINEMA | emergency-mute | 1024 | 13050600.00 / 28347300.00 / 30376500.00 | 2284350.00 / 4956600.00 / 5337300.00 | 0.1750 |
| Crossover | CINEMA | topology-disable | 1024 | 13025100.00 / 24698100.00 / 25481100.00 | 2392350.00 / 6120900.00 / 6656100.00 | 0.1837 |
| AudioGraph | CINEMA | topology-disable | 1024 | 13831800.00 / 28308300.00 / 30553500.00 | 2891700.00 / 6411000.00 / 6578100.00 | 0.2091 |
| Crossover | CINEMA | IIR-FIR-transition | 1024 | 11288100.00 / 17277000.00 / 17811000.00 | 6155100.00 / 10356600.00 / 12268800.00 | 0.5453 |
| AudioGraph | CINEMA | IIR-FIR-transition | 1024 | 11386800.00 / 17526600.00 / 19990800.00 | 6270450.00 / 10082700.00 / 11595900.00 | 0.5507 |
| Crossover | CINEMA | FIR-length-transition | 1024 | 12719400.00 / 17370900.00 / 18022800.00 | 9568500.00 / 13810800.00 / 14805900.00 | 0.7523 |
| AudioGraph | CINEMA | FIR-length-transition | 1024 | 12816000.00 / 17247300.00 / 19090500.00 | 10005450.00 / 13455900.00 / 20615100.00 | 0.7807 |
| Crossover | CINEMA | program-transition | 1024 | 18161700.00 / 24792600.00 / 27385800.00 | 13565250.00 / 19108500.00 / 19548300.00 | 0.7469 |
| AudioGraph | CINEMA | program-transition | 1024 | 18582300.00 / 23350800.00 / 25192200.00 | 13979400.00 / 18071400.00 / 19149900.00 | 0.7523 |

## Fiecare callback în timpul fade-ului

20 tranziții alternante per rând, 19.200 frames sau mai mult urmărite per tranziție. Include filtrul FIR, IIR/FIR, lungimea FIR, FIR/All-Pass și programul. Refill, control, validarea finite/active și scrierea rezultatelor sunt în afara intervalului măsurat.

| Modul | Program | Configurație | Frames | Înainte median/p99/max µs | După median/p99/max µs | Buget µs | Înainte | După |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Crossover | MUSIC | filter-fade | 128 | 1003.70 / 2695.20 / 5054.70 | 656.60 / 2028.00 / 3735.70 | 666.67 | FAIL | FAIL |
| AudioGraph | MUSIC | filter-fade | 128 | 1056.05 / 2903.20 / 4002.60 | 639.35 / 1991.00 / 3202.90 | 666.67 | FAIL | FAIL |
| Crossover | MUSIC | IIR-FIR-fade | 128 | 571.10 / 1830.20 / 3157.30 | 338.85 / 1105.80 / 2732.10 | 666.67 | FAIL | FAIL |
| AudioGraph | MUSIC | IIR-FIR-fade | 128 | 561.85 / 1763.40 / 3140.90 | 355.90 / 1036.70 / 1693.90 | 666.67 | FAIL | FAIL |
| Crossover | MUSIC | FIR-length-fade | 128 | 697.85 / 2243.20 / 30706.90 | 448.65 / 1506.70 / 2485.40 | 666.67 | FAIL | FAIL |
| AudioGraph | MUSIC | FIR-length-fade | 128 | 762.80 / 2138.00 / 3386.80 | 432.95 / 1424.50 / 2712.50 | 666.67 | FAIL | FAIL |
| Crossover | MUSIC | FIR-AllPass-fade | 128 | 666.95 / 2017.40 / 8834.70 | 413.25 / 1250.50 / 2705.40 | 666.67 | FAIL | FAIL |
| AudioGraph | MUSIC | FIR-AllPass-fade | 128 | 630.20 / 1818.90 / 2581.00 | 401.40 / 1302.70 / 2076.60 | 666.67 | FAIL | FAIL |
| Crossover | MUSIC | program-fade | 128 | 1132.50 / 2934.70 / 6995.30 | 643.75 / 2074.20 / 3498.80 | 666.67 | FAIL | FAIL |
| AudioGraph | MUSIC | program-fade | 128 | 1055.85 / 2786.60 / 5113.20 | 655.35 / 2066.90 / 3048.50 | 666.67 | FAIL | FAIL |
| Crossover | MUSIC | filter-fade | 256 | 1610.45 / 4563.80 / 39318.10 | 1080.35 / 2925.00 / 28073.80 | 1333.33 | FAIL | FAIL |
| AudioGraph | MUSIC | filter-fade | 256 | 1552.25 / 3429.40 / 4169.50 | 1154.65 / 2935.00 / 3784.80 | 1333.33 | FAIL | FAIL |
| Crossover | MUSIC | IIR-FIR-fade | 256 | 944.45 / 2411.50 / 3392.00 | 674.90 / 2192.40 / 12969.00 | 1333.33 | FAIL | FAIL |
| AudioGraph | MUSIC | IIR-FIR-fade | 256 | 958.10 / 2268.10 / 3014.60 | 628.95 / 1436.10 / 2457.50 | 1333.33 | FAIL | FAIL |
| Crossover | MUSIC | FIR-length-fade | 256 | 1068.05 / 2550.50 / 3274.10 | 743.25 / 1921.60 / 2732.90 | 1333.33 | FAIL | FAIL |
| AudioGraph | MUSIC | FIR-length-fade | 256 | 1124.45 / 2700.70 / 3464.30 | 761.75 / 1932.60 / 2579.70 | 1333.33 | FAIL | FAIL |
| Crossover | MUSIC | FIR-AllPass-fade | 256 | 955.00 / 2328.30 / 3376.50 | 657.90 / 1581.30 / 2513.80 | 1333.33 | FAIL | FAIL |
| AudioGraph | MUSIC | FIR-AllPass-fade | 256 | 989.00 / 2274.00 / 2909.80 | 698.00 / 1618.00 / 2291.50 | 1333.33 | FAIL | FAIL |
| Crossover | MUSIC | program-fade | 256 | 1553.95 / 3612.60 / 4931.20 | 1059.40 / 2705.70 / 3918.90 | 1333.33 | FAIL | FAIL |
| AudioGraph | MUSIC | program-fade | 256 | 1607.25 / 3532.50 / 4366.00 | 1087.60 / 2537.10 / 3748.40 | 1333.33 | FAIL | FAIL |
| Crossover | MUSIC | filter-fade | 512 | 2691.80 / 5515.70 / 8255.80 | 1752.90 / 3547.60 / 5060.20 | 2666.67 | FAIL | FAIL |
| AudioGraph | MUSIC | filter-fade | 512 | 2713.80 / 6849.40 / 28398.50 | 1893.95 / 4072.50 / 4785.90 | 2666.67 | FAIL | FAIL |
| Crossover | MUSIC | IIR-FIR-fade | 512 | 1763.05 / 3685.90 / 4242.40 | 1169.05 / 2454.30 / 3388.90 | 2666.67 | FAIL | FAIL |
| AudioGraph | MUSIC | IIR-FIR-fade | 512 | 1710.00 / 3284.30 / 4769.20 | 1162.90 / 2312.50 / 2782.80 | 2666.67 | FAIL | FAIL |
| Crossover | MUSIC | FIR-length-fade | 512 | 1880.15 / 3642.80 / 5227.60 | 1378.60 / 2716.80 / 3169.60 | 2666.67 | FAIL | FAIL |
| AudioGraph | MUSIC | FIR-length-fade | 512 | 1959.60 / 3683.30 / 4315.90 | 1423.70 / 2795.70 / 3157.30 | 2666.67 | FAIL | FAIL |
| Crossover | MUSIC | FIR-AllPass-fade | 512 | 1637.05 / 3409.80 / 5581.10 | 1304.75 / 2767.60 / 5234.30 | 2666.67 | FAIL | FAIL |
| AudioGraph | MUSIC | FIR-AllPass-fade | 512 | 1722.05 / 3517.30 / 5700.50 | 1363.80 / 2772.80 / 3410.20 | 2666.67 | FAIL | FAIL |
| Crossover | MUSIC | program-fade | 512 | 2572.35 / 4826.80 / 6820.00 | 1880.30 / 4039.00 / 4685.10 | 2666.67 | FAIL | FAIL |
| AudioGraph | MUSIC | program-fade | 512 | 2515.75 / 5502.50 / 6705.60 | 1855.75 / 3819.40 / 4912.50 | 2666.67 | FAIL | FAIL |
| Crossover | MUSIC | filter-fade | 1024 | 4698.45 / 10787.50 / 23278.90 | 3384.40 / 5521.00 / 6101.00 | 5333.33 | FAIL | FAIL |
| AudioGraph | MUSIC | filter-fade | 1024 | 4722.25 / 7208.20 / 7917.70 | 3493.35 / 5709.10 / 5901.60 | 5333.33 | FAIL | FAIL |
| Crossover | MUSIC | IIR-FIR-fade | 1024 | 3101.40 / 5277.90 / 5833.20 | 2189.00 / 3744.10 / 4193.90 | 5333.33 | FAIL | PASS |
| AudioGraph | MUSIC | IIR-FIR-fade | 1024 | 3116.40 / 5233.30 / 6306.90 | 2318.65 / 4353.70 / 5227.00 | 5333.33 | FAIL | PASS |
| Crossover | MUSIC | FIR-length-fade | 1024 | 3456.90 / 5489.60 / 5691.30 | 2609.80 / 4176.10 / 5678.80 | 5333.33 | FAIL | FAIL |
| AudioGraph | MUSIC | FIR-length-fade | 1024 | 3515.65 / 5400.80 / 5920.90 | 2741.15 / 4780.60 / 5337.10 | 5333.33 | FAIL | FAIL |
| Crossover | MUSIC | FIR-AllPass-fade | 1024 | 2958.85 / 4847.40 / 5044.70 | 2312.00 / 4072.20 / 6785.30 | 5333.33 | PASS | FAIL |
| AudioGraph | MUSIC | FIR-AllPass-fade | 1024 | 3366.60 / 5516.70 / 6014.40 | 2391.65 / 3820.60 / 4123.70 | 5333.33 | FAIL | PASS |
| Crossover | MUSIC | program-fade | 1024 | 4664.40 / 7473.40 / 7968.90 | 3448.50 / 5545.80 / 5854.20 | 5333.33 | FAIL | FAIL |
| AudioGraph | MUSIC | program-fade | 1024 | 4697.05 / 7627.80 / 9924.90 | 3480.00 / 6002.20 / 6135.50 | 5333.33 | FAIL | FAIL |
| Crossover | CINEMA | filter-fade | 128 | 1035.70 / 2916.90 / 4415.00 | 645.80 / 2081.80 / 3301.00 | 666.67 | FAIL | FAIL |
| AudioGraph | CINEMA | filter-fade | 128 | 1034.30 / 2907.50 / 4082.30 | 646.35 / 2071.00 / 3193.10 | 666.67 | FAIL | FAIL |
| Crossover | CINEMA | IIR-FIR-fade | 128 | 588.90 / 1781.20 / 2570.30 | 334.80 / 1056.20 / 3615.60 | 666.67 | FAIL | FAIL |
| AudioGraph | CINEMA | IIR-FIR-fade | 128 | 592.10 / 1987.50 / 3868.90 | 342.60 / 1075.70 / 2041.40 | 666.67 | FAIL | FAIL |
| Crossover | CINEMA | FIR-length-fade | 128 | 669.75 / 2088.30 / 2722.70 | 440.40 / 1338.50 / 3474.70 | 666.67 | FAIL | FAIL |
| AudioGraph | CINEMA | FIR-length-fade | 128 | 670.45 / 2043.30 / 4366.90 | 455.45 / 1385.40 / 2065.10 | 666.67 | FAIL | FAIL |
| Crossover | CINEMA | FIR-AllPass-fade | 128 | 611.60 / 1821.80 / 3241.70 | 374.50 / 1230.90 / 2634.70 | 666.67 | FAIL | FAIL |
| AudioGraph | CINEMA | FIR-AllPass-fade | 128 | 667.05 / 1966.20 / 7138.20 | 421.80 / 1346.40 / 3658.60 | 666.67 | FAIL | FAIL |
| Crossover | CINEMA | program-fade | 128 | 1069.55 / 3087.10 / 22738.90 | 623.40 / 2081.90 / 3966.40 | 666.67 | FAIL | FAIL |
| AudioGraph | CINEMA | program-fade | 128 | 1055.15 / 2859.70 / 4789.60 | 669.70 / 1976.80 / 3795.60 | 666.67 | FAIL | FAIL |
| Crossover | CINEMA | filter-fade | 256 | 1582.15 / 3647.80 / 4359.80 | 1047.65 / 2591.40 / 3535.30 | 1333.33 | FAIL | FAIL |
| AudioGraph | CINEMA | filter-fade | 256 | 1607.40 / 3645.10 / 5112.30 | 1060.55 / 2877.00 / 8807.90 | 1333.33 | FAIL | FAIL |
| Crossover | CINEMA | IIR-FIR-fade | 256 | 894.85 / 2287.80 / 3282.30 | 630.45 / 1659.00 / 2316.40 | 1333.33 | FAIL | FAIL |
| AudioGraph | CINEMA | IIR-FIR-fade | 256 | 920.70 / 2369.40 / 4252.70 | 685.20 / 1770.40 / 2393.30 | 1333.33 | FAIL | FAIL |
| Crossover | CINEMA | FIR-length-fade | 256 | 1083.00 / 2915.80 / 4086.40 | 757.10 / 1991.50 / 2905.80 | 1333.33 | FAIL | FAIL |
| AudioGraph | CINEMA | FIR-length-fade | 256 | 1141.25 / 2939.60 / 4419.50 | 786.85 / 1905.60 / 2550.80 | 1333.33 | FAIL | FAIL |
| Crossover | CINEMA | FIR-AllPass-fade | 256 | 960.05 / 2345.50 / 3204.10 | 654.30 / 1541.10 / 3011.00 | 1333.33 | FAIL | FAIL |
| AudioGraph | CINEMA | FIR-AllPass-fade | 256 | 998.25 / 2255.70 / 3012.00 | 688.90 / 1809.20 / 2418.30 | 1333.33 | FAIL | FAIL |
| Crossover | CINEMA | program-fade | 256 | 1551.10 / 3449.40 / 4930.80 | 1052.60 / 2631.50 / 3687.70 | 1333.33 | FAIL | FAIL |
| AudioGraph | CINEMA | program-fade | 256 | 1593.85 / 3603.00 / 4792.50 | 1076.95 / 2768.80 / 3902.60 | 1333.33 | FAIL | FAIL |
| Crossover | CINEMA | filter-fade | 512 | 2637.15 / 5167.70 / 7365.80 | 1866.00 / 4104.80 / 4774.40 | 2666.67 | FAIL | FAIL |
| AudioGraph | CINEMA | filter-fade | 512 | 2658.75 / 4908.30 / 6609.20 | 1872.05 / 3889.50 / 4916.70 | 2666.67 | FAIL | FAIL |
| Crossover | CINEMA | IIR-FIR-fade | 512 | 1590.05 / 3178.60 / 4181.00 | 1149.10 / 2389.50 / 3000.00 | 2666.67 | FAIL | FAIL |
| AudioGraph | CINEMA | IIR-FIR-fade | 512 | 1657.20 / 3347.10 / 4319.80 | 1212.25 / 2527.90 / 3058.80 | 2666.67 | FAIL | FAIL |
| Crossover | CINEMA | FIR-length-fade | 512 | 1913.05 / 3687.40 / 4433.50 | 1387.00 / 2964.90 / 3465.70 | 2666.67 | FAIL | FAIL |
| AudioGraph | CINEMA | FIR-length-fade | 512 | 1928.55 / 4011.90 / 5211.50 | 1376.00 / 2981.90 / 4443.80 | 2666.67 | FAIL | FAIL |
| Crossover | CINEMA | FIR-AllPass-fade | 512 | 1677.95 / 3291.90 / 4536.70 | 1213.85 / 2713.90 / 3075.80 | 2666.67 | FAIL | FAIL |
| AudioGraph | CINEMA | FIR-AllPass-fade | 512 | 1725.00 / 3330.60 / 4078.40 | 1322.20 / 2624.30 / 3124.50 | 2666.67 | FAIL | FAIL |
| Crossover | CINEMA | program-fade | 512 | 2566.65 / 4821.50 / 5583.80 | 1862.10 / 3799.30 / 5197.90 | 2666.67 | FAIL | FAIL |
| AudioGraph | CINEMA | program-fade | 512 | 2598.95 / 5026.90 / 5735.30 | 1918.30 / 3787.90 / 5152.30 | 2666.67 | FAIL | FAIL |
| Crossover | CINEMA | filter-fade | 1024 | 4665.30 / 7512.60 / 8190.70 | 3439.25 / 5291.70 / 5820.50 | 5333.33 | FAIL | FAIL |
| AudioGraph | CINEMA | filter-fade | 1024 | 4760.25 / 7370.40 / 7836.30 | 3508.15 / 5495.60 / 6621.70 | 5333.33 | FAIL | FAIL |
| Crossover | CINEMA | IIR-FIR-fade | 1024 | 3071.00 / 5003.00 / 5908.80 | 2250.25 / 3971.70 / 4334.20 | 5333.33 | FAIL | PASS |
| AudioGraph | CINEMA | IIR-FIR-fade | 1024 | 3119.55 / 5545.50 / 6123.70 | 2275.85 / 3546.60 / 4029.30 | 5333.33 | FAIL | PASS |
| Crossover | CINEMA | FIR-length-fade | 1024 | 3516.35 / 5937.60 / 7290.10 | 2566.55 / 4143.40 / 5374.70 | 5333.33 | FAIL | FAIL |
| AudioGraph | CINEMA | FIR-length-fade | 1024 | 3555.35 / 5595.60 / 6108.80 | 2711.00 / 4574.40 / 5051.90 | 5333.33 | FAIL | PASS |
| Crossover | CINEMA | FIR-AllPass-fade | 1024 | 3143.95 / 8016.70 / 49869.70 | 2312.90 / 3830.70 / 4449.10 | 5333.33 | FAIL | PASS |
| AudioGraph | CINEMA | FIR-AllPass-fade | 1024 | 3239.80 / 5623.30 / 7072.70 | 2353.85 / 3666.70 / 4429.70 | 5333.33 | FAIL | PASS |
| Crossover | CINEMA | program-fade | 1024 | 4636.50 / 7633.10 / 8734.40 | 3398.95 / 5679.30 / 6394.70 | 5333.33 | FAIL | FAIL |
| AudioGraph | CINEMA | program-fade | 1024 | 4824.00 / 7321.30 / 8314.80 | 3485.00 / 5940.40 / 6490.50 | 5333.33 | FAIL | FAIL |

## Cicluri CPU în fade

| Modul | Program | Configurație | Frames | Înainte median/p99/max cycles | După median/p99/max cycles | Ratio median după/înainte |
| --- | --- | --- | --- | --- | --- | --- |
| Crossover | MUSIC | filter-fade | 128 | 2967000.00 / 7737900.00 / 10707300.00 | 1953450.00 / 5499900.00 / 9252900.00 | 0.6584 |
| AudioGraph | MUSIC | filter-fade | 128 | 3097650.00 / 8183100.00 / 11457900.00 | 1877400.00 / 5523900.00 / 8708100.00 | 0.6061 |
| Crossover | MUSIC | IIR-FIR-fade | 128 | 1694250.00 / 4997400.00 / 8908800.00 | 1012050.00 / 3118200.00 / 8183400.00 | 0.5973 |
| AudioGraph | MUSIC | IIR-FIR-fade | 128 | 1663200.00 / 4853400.00 / 9117000.00 | 1065750.00 / 2902800.00 / 4348500.00 | 0.6408 |
| Crossover | MUSIC | FIR-length-fade | 128 | 2070900.00 / 5917500.00 / 10112100.00 | 1330800.00 / 4169700.00 / 5981100.00 | 0.6426 |
| AudioGraph | MUSIC | FIR-length-fade | 128 | 2264550.00 / 5838000.00 / 7519800.00 | 1279200.00 / 3910500.00 / 7092300.00 | 0.5649 |
| Crossover | MUSIC | FIR-AllPass-fade | 128 | 1972650.00 / 5306100.00 / 7637700.00 | 1216350.00 / 3365100.00 / 4747500.00 | 0.6166 |
| AudioGraph | MUSIC | FIR-AllPass-fade | 128 | 1867050.00 / 4985700.00 / 7672500.00 | 1181850.00 / 3499200.00 / 5778300.00 | 0.6330 |
| Crossover | MUSIC | program-fade | 128 | 3302100.00 / 8281800.00 / 13463100.00 | 1885200.00 / 5793000.00 / 9121200.00 | 0.5709 |
| AudioGraph | MUSIC | program-fade | 128 | 3131850.00 / 7774800.00 / 12108000.00 | 1913400.00 / 5762100.00 / 8739300.00 | 0.6109 |
| Crossover | MUSIC | filter-fade | 256 | 4666350.00 / 10326300.00 / 12905400.00 | 3185550.00 / 7229100.00 / 9320400.00 | 0.6827 |
| AudioGraph | MUSIC | filter-fade | 256 | 4584600.00 / 9885300.00 / 11421300.00 | 3382350.00 / 7979100.00 / 11174700.00 | 0.7378 |
| Crossover | MUSIC | IIR-FIR-fade | 256 | 2817600.00 / 6729900.00 / 9486600.00 | 1978950.00 / 4623900.00 / 5742000.00 | 0.7024 |
| AudioGraph | MUSIC | IIR-FIR-fade | 256 | 2853150.00 / 6457500.00 / 8471700.00 | 1867350.00 / 4053600.00 / 6469200.00 | 0.6545 |
| Crossover | MUSIC | FIR-length-fade | 256 | 3181050.00 / 7239300.00 / 9124800.00 | 2212500.00 / 5302200.00 / 7533900.00 | 0.6955 |
| AudioGraph | MUSIC | FIR-length-fade | 256 | 3330150.00 / 7580700.00 / 8554200.00 | 2270250.00 / 5458200.00 / 6877800.00 | 0.6817 |
| Crossover | MUSIC | FIR-AllPass-fade | 256 | 2813700.00 / 6382200.00 / 8081400.00 | 1957200.00 / 4248600.00 / 6350100.00 | 0.6956 |
| AudioGraph | MUSIC | FIR-AllPass-fade | 256 | 2928900.00 / 6312000.00 / 8236800.00 | 2080650.00 / 4503600.00 / 6309600.00 | 0.7104 |
| Crossover | MUSIC | program-fade | 256 | 4595850.00 / 10241700.00 / 14249700.00 | 3121650.00 / 7456500.00 / 10740300.00 | 0.6792 |
| AudioGraph | MUSIC | program-fade | 256 | 4728300.00 / 9845400.00 / 12022800.00 | 3211800.00 / 7207500.00 / 10387500.00 | 0.6793 |
| Crossover | MUSIC | filter-fade | 512 | 7895400.00 / 14883900.00 / 16703700.00 | 5161350.00 / 9919200.00 / 14470500.00 | 0.6537 |
| AudioGraph | MUSIC | filter-fade | 512 | 7896300.00 / 15294300.00 / 17625300.00 | 5541000.00 / 10693200.00 / 13974000.00 | 0.7017 |
| Crossover | MUSIC | IIR-FIR-fade | 512 | 5258400.00 / 10333200.00 / 12012900.00 | 3464400.00 / 7043700.00 / 8954700.00 | 0.6588 |
| AudioGraph | MUSIC | IIR-FIR-fade | 512 | 5067900.00 / 9275400.00 / 13266900.00 | 3470550.00 / 6631500.00 / 8033700.00 | 0.6848 |
| Crossover | MUSIC | FIR-length-fade | 512 | 5587950.00 / 10564200.00 / 12113700.00 | 4094850.00 / 7634400.00 / 8556300.00 | 0.7328 |
| AudioGraph | MUSIC | FIR-length-fade | 512 | 5781900.00 / 10311000.00 / 11730900.00 | 4234500.00 / 7986900.00 / 9477000.00 | 0.7324 |
| Crossover | MUSIC | FIR-AllPass-fade | 512 | 4792050.00 / 9482700.00 / 14311200.00 | 3835950.00 / 7015200.00 / 7938900.00 | 0.8005 |
| AudioGraph | MUSIC | FIR-AllPass-fade | 512 | 5097150.00 / 9482700.00 / 15052800.00 | 4027950.00 / 7244700.00 / 8095800.00 | 0.7902 |
| Crossover | MUSIC | program-fade | 512 | 7662000.00 / 14063100.00 / 16849800.00 | 5566500.00 / 11412600.00 / 12001500.00 | 0.7265 |
| AudioGraph | MUSIC | program-fade | 512 | 7388400.00 / 15076200.00 / 18078900.00 | 5524950.00 / 10671600.00 / 13755900.00 | 0.7478 |
| Crossover | MUSIC | filter-fade | 1024 | 14035650.00 / 23361300.00 / 26980800.00 | 9947400.00 / 15793500.00 / 17271900.00 | 0.7087 |
| AudioGraph | MUSIC | filter-fade | 1024 | 14076300.00 / 20762400.00 / 22172700.00 | 10371600.00 / 16289700.00 / 17472900.00 | 0.7368 |
| Crossover | MUSIC | IIR-FIR-fade | 1024 | 9239850.00 / 14498100.00 / 15956400.00 | 6447000.00 / 10513200.00 / 11374500.00 | 0.6977 |
| AudioGraph | MUSIC | IIR-FIR-fade | 1024 | 9291450.00 / 14374800.00 / 17152200.00 | 6837750.00 / 11828400.00 / 13339800.00 | 0.7359 |
| Crossover | MUSIC | FIR-length-fade | 1024 | 10305600.00 / 15637200.00 / 16776900.00 | 7748700.00 / 11891100.00 / 12763800.00 | 0.7519 |
| AudioGraph | MUSIC | FIR-length-fade | 1024 | 10389450.00 / 15292800.00 / 16732500.00 | 8136450.00 / 13350300.00 / 13854000.00 | 0.7831 |
| Crossover | MUSIC | FIR-AllPass-fade | 1024 | 8711400.00 / 13433400.00 / 14571600.00 | 6814800.00 / 10769700.00 / 12488700.00 | 0.7823 |
| AudioGraph | MUSIC | FIR-AllPass-fade | 1024 | 9796800.00 / 14831400.00 / 16409400.00 | 7063500.00 / 10927200.00 / 12126900.00 | 0.7210 |
| Crossover | MUSIC | program-fade | 1024 | 13948200.00 / 20988300.00 / 22508400.00 | 10253700.00 / 15845400.00 / 17226900.00 | 0.7351 |
| AudioGraph | MUSIC | program-fade | 1024 | 13928100.00 / 22101900.00 / 24021900.00 | 10375950.00 / 17355000.00 / 18072900.00 | 0.7450 |
| Crossover | CINEMA | filter-fade | 128 | 3069600.00 / 8166000.00 / 12366000.00 | 1915950.00 / 5856000.00 / 8337600.00 | 0.6242 |
| AudioGraph | CINEMA | filter-fade | 128 | 3075900.00 / 8126400.00 / 10842900.00 | 1906800.00 / 5843400.00 / 9347100.00 | 0.6199 |
| Crossover | CINEMA | IIR-FIR-fade | 128 | 1742700.00 / 5032800.00 / 7267200.00 | 998400.00 / 2885700.00 / 4823100.00 | 0.5729 |
| AudioGraph | CINEMA | IIR-FIR-fade | 128 | 1754100.00 / 5634300.00 / 11300400.00 | 1019700.00 / 2921700.00 / 4645800.00 | 0.5813 |
| Crossover | CINEMA | FIR-length-fade | 128 | 1984500.00 / 5704200.00 / 7770900.00 | 1308300.00 / 3880800.00 / 5770200.00 | 0.6593 |
| AudioGraph | CINEMA | FIR-length-fade | 128 | 1991850.00 / 5813400.00 / 12830700.00 | 1355250.00 / 3860700.00 / 5409600.00 | 0.6804 |
| Crossover | CINEMA | FIR-AllPass-fade | 128 | 1817850.00 / 4901700.00 / 8655300.00 | 1115700.00 / 3439200.00 / 6828000.00 | 0.6137 |
| AudioGraph | CINEMA | FIR-AllPass-fade | 128 | 1978050.00 / 5312400.00 / 7622700.00 | 1253400.00 / 3646500.00 / 5855700.00 | 0.6337 |
| Crossover | CINEMA | program-fade | 128 | 3141300.00 / 7966500.00 / 11614800.00 | 1849800.00 / 5802900.00 / 9927600.00 | 0.5889 |
| AudioGraph | CINEMA | program-fade | 128 | 3130800.00 / 7965000.00 / 11257800.00 | 1986450.00 / 5552400.00 / 8976000.00 | 0.6345 |
| Crossover | CINEMA | filter-fade | 256 | 4666950.00 / 10044300.00 / 12646200.00 | 3093150.00 / 7346700.00 / 9979200.00 | 0.6628 |
| AudioGraph | CINEMA | filter-fade | 256 | 4738200.00 / 10207200.00 / 13207800.00 | 3136800.00 / 7332000.00 / 11120100.00 | 0.6620 |
| Crossover | CINEMA | IIR-FIR-fade | 256 | 2660700.00 / 6426600.00 / 9003900.00 | 1855800.00 / 4519800.00 / 5811300.00 | 0.6975 |
| AudioGraph | CINEMA | IIR-FIR-fade | 256 | 2740650.00 / 6371400.00 / 11387100.00 | 2050650.00 / 4788900.00 / 6033300.00 | 0.7482 |
| Crossover | CINEMA | FIR-length-fade | 256 | 3201900.00 / 7833000.00 / 9914400.00 | 2264700.00 / 5657700.00 / 7633800.00 | 0.7073 |
| AudioGraph | CINEMA | FIR-length-fade | 256 | 3386400.00 / 7941900.00 / 11191500.00 | 2336400.00 / 5316300.00 / 6644400.00 | 0.6899 |
| Crossover | CINEMA | FIR-AllPass-fade | 256 | 2811750.00 / 6466200.00 / 8760600.00 | 1947000.00 / 4462200.00 / 7238400.00 | 0.6925 |
| AudioGraph | CINEMA | FIR-AllPass-fade | 256 | 2933100.00 / 6439500.00 / 8507100.00 | 2040750.00 / 4933500.00 / 6476100.00 | 0.6958 |
| Crossover | CINEMA | program-fade | 256 | 4567350.00 / 9791400.00 / 13425600.00 | 3116850.00 / 7404900.00 / 9607200.00 | 0.6824 |
| AudioGraph | CINEMA | program-fade | 256 | 4729200.00 / 9947700.00 / 14347500.00 | 3216300.00 / 7956300.00 / 10548300.00 | 0.6801 |
| Crossover | CINEMA | filter-fade | 512 | 7780650.00 / 14311200.00 / 17711400.00 | 5455050.00 / 10491900.00 / 12345600.00 | 0.7011 |
| AudioGraph | CINEMA | filter-fade | 512 | 7896600.00 / 14014200.00 / 17516100.00 | 5550450.00 / 10757400.00 / 12874500.00 | 0.7029 |
| Crossover | CINEMA | IIR-FIR-fade | 512 | 4727100.00 / 8874000.00 / 12047100.00 | 3402000.00 / 6596700.00 / 7597200.00 | 0.7197 |
| AudioGraph | CINEMA | IIR-FIR-fade | 512 | 4940550.00 / 9445200.00 / 12720600.00 | 3587550.00 / 6729000.00 / 8446200.00 | 0.7261 |
| Crossover | CINEMA | FIR-length-fade | 512 | 5658150.00 / 9795300.00 / 11586600.00 | 4096950.00 / 8001300.00 / 10106100.00 | 0.7241 |
| AudioGraph | CINEMA | FIR-length-fade | 512 | 5724600.00 / 11118000.00 / 14302200.00 | 4088100.00 / 8310300.00 / 13013700.00 | 0.7141 |
| Crossover | CINEMA | FIR-AllPass-fade | 512 | 4962150.00 / 9327000.00 / 13325400.00 | 3594000.00 / 6923700.00 / 8645700.00 | 0.7243 |
| AudioGraph | CINEMA | FIR-AllPass-fade | 512 | 5098200.00 / 9162300.00 / 11046000.00 | 3901050.00 / 7171800.00 / 8000700.00 | 0.7652 |
| Crossover | CINEMA | program-fade | 512 | 7603650.00 / 13378800.00 / 16599000.00 | 5491350.00 / 10809300.00 / 14597400.00 | 0.7222 |
| AudioGraph | CINEMA | program-fade | 512 | 7641600.00 / 14558700.00 / 16485300.00 | 5690550.00 / 10916400.00 / 13621800.00 | 0.7447 |
| Crossover | CINEMA | filter-fade | 1024 | 13917000.00 / 21102300.00 / 22302600.00 | 10250850.00 / 15581400.00 / 16679400.00 | 0.7366 |
| AudioGraph | CINEMA | filter-fade | 1024 | 14158500.00 / 20746200.00 / 21449400.00 | 10435800.00 / 15711600.00 / 17881800.00 | 0.7371 |
| Crossover | CINEMA | IIR-FIR-fade | 1024 | 9117300.00 / 14277900.00 / 16827000.00 | 6696750.00 / 11066100.00 / 11187000.00 | 0.7345 |
| AudioGraph | CINEMA | IIR-FIR-fade | 1024 | 9252600.00 / 14953500.00 / 16446600.00 | 6755100.00 / 9992100.00 / 10413900.00 | 0.7301 |
| Crossover | CINEMA | FIR-length-fade | 1024 | 10445400.00 / 15967200.00 / 20877300.00 | 7634850.00 / 11927100.00 / 15969000.00 | 0.7309 |
| AudioGraph | CINEMA | FIR-length-fade | 1024 | 10483050.00 / 16111200.00 / 17440200.00 | 8047200.00 / 12977700.00 / 14076300.00 | 0.7676 |
| Crossover | CINEMA | FIR-AllPass-fade | 1024 | 9218100.00 / 14444100.00 / 19389000.00 | 6734250.00 / 10539900.00 / 12310500.00 | 0.7305 |
| AudioGraph | CINEMA | FIR-AllPass-fade | 1024 | 9523800.00 / 14765400.00 / 15409200.00 | 6907500.00 / 10388100.00 / 11126100.00 | 0.7253 |
| Crossover | CINEMA | program-fade | 1024 | 13746300.00 / 21324000.00 / 23736900.00 | 10111350.00 / 16767600.00 / 18868200.00 | 0.7356 |
| AudioGraph | CINEMA | program-fade | 1024 | 14050200.00 / 20406600.00 / 23740200.00 | 10383750.00 / 16651500.00 / 17385900.00 | 0.7390 |
