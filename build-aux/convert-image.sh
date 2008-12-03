#!/bin/sh

outname="images.h"

# Process options (output filename)
while getopts "o:" option
do
  case $option in
    o) outname=$OPTARG ;;
    *) exit 1 ;;
  esac
done

shift $(($OPTIND - 1))

# Create the macro name for the header file
macroname=`echo $outname | tr "[:lower:]" "[:upper:]" | tr -c "[:alnum:]" "_"`

# write the header
echo "#ifndef "$macroname > $outname
echo "#define "$macroname >> $outname
echo "#include <gdk-pixbuf/gdk-pixdata.h>" >> $outname

# convert every supplied file
for file in $@
do
  base=`basename $file .png`
  cname=pix_$base
  echo $file

  gdk-pixbuf-csource --raw --struct --name=$cname $file >> $outname
done

# write footer of the file
echo "#endif" >> $outname

