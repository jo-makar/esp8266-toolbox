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
    #smtpclass, port = smtplib.SMTP, 587
    smtpclass, port = smtplib.SMTP_SSL, 465

    smtp = smtpclass('smtp.gmail.com', port)
    smtp.set_debuglevel(True)

    smtp.ehlo()
    if port == 587:
        smtp.starttls()
    smtp.login(username, password)
    smtp.sendmail(mailfrom, mailto, '\r\n'.join(['From: %s' % mailfrom,
                                                 'To: %s' % mailto,
                                                 'Subject: test',
                                                 '',
                                                 'test']))
    smtp.quit()
