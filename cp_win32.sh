# /bin/sh

# copy a file to the XP machine for testing

ftp xp << EOT
cd /Users/me/Desktop/xplane/Resources/plugins
bin
put $1
EOT

