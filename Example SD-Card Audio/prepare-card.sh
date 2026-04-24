#!/bin/bash

# Bash script for Apple Mac. Run in a console window. Currently only works for .wav files only!
# Renames the .wav audio filenames to numbers and keeps the file extentions.
# Also cleans up possible hidden files (file name starting with '.') that may confuse the MP3 player.
#
# AvD nov 2019 ~ april 2026
#
#subname=1

# Get rid of hidden directories...
find . -type d -name '.[_|DS]*' -exec rm -rf {} \;

# ToDo: rename the subdirs....
#for subdir in $(find . -maxdepth 1 -type d|sort)
#do
#  if [[ "$subdir" == "." ]]; then
#    continue
#  fi
#  mv $subdir 0$subname
#  echo "$subdir was moved to 0$subname"
#  ((subname=subname+1))
#done

# Now do the individual files with .wav file extension...
for subdir in $(find . -maxdepth 1 -type d|sort)
do
  if [[ "$subdir" == "." ]]; then
    continue
  fi
  filecntr=1;
  for wavfile in $subdir/*.wav ; do
    if [[ "$wavfile" == "." ]]; then
      continue
    fi
    if [[ "$wavfile" == ".." ]]; then
      continue
    fi
    safename=${wavfile// /_}
    mv "$wavfile" "${wavfile// /_}"
    if [ $filecntr -lt 10 ]; then
      mv $safename $subdir/00${filecntr}.wav
    else
      mv $safename $subdir/0${filecntr}.wav    
    fi
    ((filecntr=filecntr+1))
  done
done

# Finally clean up hidden-/meta-files...
find . -type f -name '.[_|DS]*' -exec rm {} \;
echo "All done..."
