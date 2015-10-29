#!/bin/bash
egrep "recv_size|Register|Wriet|Derigester|dt1|dt2|dt3" client1.log | paste  - - - - - - - | awk '{print $6, $3, $9, $13, $17, $20, $23}'
#egrep "recv_size|Register|Write|Derigester|dt1|dt2|dt3" client.log | paste  - - - - - - - | awk '{print $6, $3, $9, $13, $17, $20, $23}'
#egrep "recv_size|Register|Wriet|Derigester|job_usec" client1.log | paste - - - - - | awk '{print $6, $3, $9, $13, $17, $20, $23, $21}' | sed -r 's/=/ /g' | sed -r 's/job_usec//g'
