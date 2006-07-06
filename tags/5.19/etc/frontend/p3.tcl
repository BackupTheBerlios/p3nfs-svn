#!/usr/bin/wishx

# Change the following 4 lines to meet your preference.
set mntdir	/mnt/psion
set imgpath	. ;		# The 2 gifs are living here
set usekde	1 ;		# Set this to 1 to start kfm in $mntdir
set p3nfs "/local/bin/p3nfsd -series5 -dir $mntdir"

image create photo p3_0 -file [ join "$imgpath p3_off.gif" / ]
image create photo p3_1 -file [ join "$imgpath p3_on.gif" / ]

set mounted 0
wm title . "p3nfs"

proc checkmount {} {
  global mntdir .t.t mounted
  set mounted [expr 1 - [catch "exec mount | grep $mntdir"]]
  .t.t configure -image p3_$mounted
  after 10000 checkmount
}

proc domount {} {
  global mounted mntdir p3nfs .t usekde
  checkmount
  if { $mounted } {
    catch "exec ls $mntdir/exit"
  } else {
    catch "exec $p3nfs > /dev/console 2> /dev/console < /dev/null &"
  }
  sleep 3
  checkmount
  if { $mounted && $usekde } { catch "exec kfmclient openURL file:$mntdir/" }
}

frame .t -borderwidth 0
button .t.t -image p3_1 -bd 1 -relief raised -command domount
pack .t.t .t
checkmount
