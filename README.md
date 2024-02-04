Basic configuration:

  --suffix <_a|a|0|_b|b|1\>
  
      By default, it will be taken from ro.boot.slot_suffix

  --slot <0|1>
  
      Slot 0 will be used by default
  --super </path/to/super\>
  
      By default, the standard path /dev/block/by-name/super will be used

  --group <group_name_in_super\>
  
      By default, the relative section of system + --suffix in the specified --slot will be searched

Please use one of the following options:

  --create <partition name\> <partition size\>
  
  --remove <partition name\>
  
  --resize <partition name\> <newsize\>
  
  --replace <original partition name\> <new partition name\>
  
  --map <partition name\>
  
  --unmap <partition name\>
  
  --free
  
  --unlimited-group
  
  --clear-cow

  --get-info
  
