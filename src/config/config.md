#### Record only be triggerd by arm|desarm pin
`use_enable_pin` `true`|`false`
#### Record as soon as module is powered
`always_rcd` `true`|`false`
#### Percentage factor used to tune mic gain, <100 -> lower the gain  >100 maximise the gain, typical values
`mic_gain`  `10`~`110`
#### Use high pass filter
`use_high_pass_filter` `true`|`false`
#### Cutoff frequency of numeric high pass filter
`high_pass_cutoff_freq` `50`~`500`
#### i2s record sample rate
`sample_rate` `22080`|`44180`
#### Autoincremented index used in file name to ensure unicity
`next_file_name_index` `1`~`...`
#### WAV files destination folder
`rcd_folder` `/`|`records/`|`abcdef/`
#### WAV file base name;
`rcd_file_name` `mic_wav`|`abcdef`
#### Delete all WAV fiels from destination folder if 'enable' state is toggle 3 times in 5 seconds row
#### This will work only if on Disarm state, Arming will result on abort of delete attempt
`delete_on_multiple_enable_tick` `true`|`false`
