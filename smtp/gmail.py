#!/usr/bin/env python

# Send email using the Gmail SMTP servers
#
# Login troubleshooting:
# https://support.google.com/mail/answer/7126229?p=BadCredentials#cantsignin

import smtplib

username = 'xxx@gmail.com'
password = 'yyy'
mailfrom = mailto = username

if __name__ == '__main__':
    smtp = smtplib.SMTP('smtp.gmail.com:587')
    smtp.set_debuglevel(True)

    smtp.ehlo()
    smtp.starttls()
    smtp.login(username, password)
    smtp.sendmail(mailfrom, mailto, '\r\n'.join(['From: %s' % mailfrom,
                                                 'To: %s' % mailto,
                                                 'Subject: test',
                                                 '',
                                                 'test']))
    smtp.quit()
