#!/usr/bin/env python

# Send email using the SMTP2GO SMTP servers

import smtplib

username = 'xxx'
password = 'yyy'
mailfrom = mailto = 'zzz'

if __name__ == '__main__':
    smtp = smtplib.SMTP('mail.smtp2go.com', 2525)
    smtp.set_debuglevel(True)

    smtp.ehlo()
    smtp.login(username, password)
    smtp.sendmail(mailfrom, mailto, '\r\n'.join(['From: %s' % mailfrom,
                                                 'To: %s' % mailto,
                                                 'Subject: test',
                                                 '',
                                                 'test']))
    smtp.quit()
