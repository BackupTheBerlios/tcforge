#!/usr/bin/gawk -f

#  "strength", "Blending factor.", "%f", "0.9", "0.0", "2.0"
#  "range", "Apply the filter only to a range", "%u-%u", "0-0", "0", "-1", "0", "-1"
#  "pre", "Run as a PRE filter", "", "0"
#  "log", "Logfile for verbose info", "%s", "/tmp/logfile"


# "filter_logoaway.so", "remove an image from the video", "v0.1.0 (2002-12-04)", "Thomas Wehrspann", "VRY", "1"
# "range", "Frame Range", "%d-%d", "0-0", "0", "-1", "0", "-1"
# "pos", "Position of logo", "%dx%d", "0x0", "0", "width", "0", "height"
# "size", "Size of logo", "%dx%d", "10x10", "0", "widht", "0", "height"
# "mode", "Filter Mode (0=none, 1=solid, 2=xy)", "%d", "0", "0", "3"
# "border", "Visible Border", "", "0"
# "xweight", "X-Y Weight(0%-100%)", "%d", "50", "0", "100"
# "fill", "Solid Fill Color(RGB)", "%x%x%x", "0x0x0", "0", "255", "0", "255", "0", "255"
# CSV parser

# NR := current Record
# NF := Number of fields

BEGIN { 

   if (ARGC <= 3) {
     print "Usage: prog INFILE FILTER_PREFIX OUTFILE"
     exit 1
   }
   fprefix = ARGV[2];
   delete ARGV[2];
   sub(/=.*/, "", fprefix);
   fstring = fprefix "="
   outputfile=ARGV[3];
   delete ARGV[3];
   init_done = 1;
   j = 1;

   FS = "\" *, *\"";
}

# first line is special
NR == 1{
   sub(/^\"/,"",$1)
   sub(/\"$/,"",$6)
   filter_name    = $1;
   filter_comment = $2;
   filter_version = $3;
   filter_author  = $4;
   capabilities   = $5;
   frames_needed  = $6;
   next
}

{
   gsub(/^\"/,"",$1)

   name[j]    = $1;
   comment[j] = $2;
   format[j]  = $3;

   if (format[j] ~ /^$/) {
     format[j] = "bool"
   }

   val[j, 0] = NF-4
   for (i=4; i <= NF; i++) {
     gsub(/"$/,"", $i)
     val[j, i-3] = $i;
   }
   j++;
}

function parameters ()
{
   while (1) {

   maxlen = 0;
   for (i in name) {
     if (maxlen < length(name[i])) {
       maxlen = length(name[i]);
     }
   }

   fmt = "%-"maxlen"s"
   
   for (i=1; i<=size; i++) { 
     str = sprintf ("%2d: "fmt" [%-3s]", i, name[i], val[i, 1]);
     #printf ("%2d: "fmt" [%-3s]  (%s)\n", i, name[i], val[i, 1], comment[i]);
     printf ("%s ", str);
     lenstr = length(str);
     lencom = length(comment[i])
     # if a comment ist longer than 75 chars, break it
     if (lenstr+lencom>=75) {
	com = comment[i];
	j=0;
	blanks = ""
	while (j <= lenstr+1) {
	   blanks = blanks " ";
	   j++;
	}
	for (j = 0; j < lencom+lenstr; j+=75) {
	   mstr = substr (com, 1, 75-lenstr);
	   com = substr (com, 75-lenstr+1, length(com));
	   if (j == 0) {
	      print "("mstr;
	   } else {
	      printf ("%s%s", blanks, mstr);
	      if (j+75 >= lencom+lenstr) {
	        printf (")\n");
	      } else {
	        printf ("\n");
	      }
	   }
	} # for
     } else {
	print "("comment[i]")";
     }

   } 

   print " ** Outputstring: "fstring
   printf ("[%d-%d (q)uit (f)ree]? ", 1, size);
   getline reply < "-"

   if (reply ~ /[Ff](ree)?/) { 
      printf ("? ");
      getline newval < "-";
      fstringold = fstring;
      fstring = fstring  newval;
      if (fstring !~ /:$/) {
	 fstring = fstring ":" 
      }
      return -1
   }

   if (reply ~ /[QqEe](uit|xit)?/) { 
     print "** Quitting ..."
     break
   }

   if (reply !~ /^[0-9]+$/) { 
     print "** Not a number"
     continue
   }

   if (reply < 1 || reply > size) { 
     print "** Invalid number" 
     continue
   }

   return reply 
   }
   return 0
}

END { 

   if (!init_done)
     exit 1
   size = j-1
   print "Parameter for " filter_name " by " filter_author
   print "-- "filter_comment" --"
   print "Version ("filter_version") ("capabilities") (need "frames_needed" frames)"
   print "----------------------------"
   if (NR == 0) {
     print "** No options available";
     printf ("[press anykey to continue]");
     getline newval < "-";
     exit 1;
   }

   doparam=1

   while (doparam){
      print "";
      print " ** Outputstring: "fstring;
      printf ("(d)one (a)dd (c)ancel (u)undo (p)rint (h)elp? ");
      getline newval < "-";

      if (newval ~ /[Cc](ancel)?/) {
	 fstring = fprefix "=";
	 continue;
      }
      else if (newval ~ /[Uu](ndo)?/) {
	 fstring = fstringold;
	 continue;
      }
      else if (newval ~ /[Dd](one)?/) {
	 printf ("%s\n", fstring) > outputfile;
	 fstring = fprefix;
	 doparam=0;
	 break
      }
      else if (newval ~ /[Aa](dd)?/) {
      }
      else if (newval ~ /[Pp](rint)?/) {
	 print "** String: "fstring;
	 continue;
      }
      else if (newval ~ /[Hh?](help)?/) {
	 print "** Help **";
	 print "(d) ... Write the string and exit";
	 print "(a) ... Add a string";
	 print "(c) ... Will delete the string you have built so far";
	 print "(u) ... Will delete the last value you have added";
	 print "(p) ... Print the string build so far";
	 print "(h) ... This help";
	 continue;
      }
      else if (newval ~ /^?/) {
	 continue;
      } else {
	 print "** Invalid response"
	 continue;
      }

      reply = parameters();
      if (reply > 0) {
	 print "----------------------------";
	 printf ("** Changing %s [%s] ** Current %s", 
	       name[reply], format[reply], val[reply,1]);

	 for (t=2; t <= val[reply,0]; t+=2) {
	    printf(" ** from %s to %s", val[reply,t], val[reply,t+1]);
	 }
	 printf ("\n");

	 printf ("Value? ");
	 getline newval < "-";

	 fstringold = fstring;
	 fstring = fstring  name[reply] "=" newval ":";

      } else if (reply < 0) {
	 # free string
      } else {
        break;
      }
   }


}
