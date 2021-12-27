import sys
import urllib2

FLUTTER_TOS_URL = 'http://tosv.byted.org/obj/toutiao.ios.arch/flutter/framework'
SO_TOS_URL = FLUTTER_TOS_URL + '/%s/%s/%s'
BUILD_ID_TOS_URL = FLUTTER_TOS_URL + '/buildid/%s'


def download_so(cid, mode):
    print('... Start download libflutter.so ...')
    url = SO_TOS_URL % (cid, mode, 'libflutter_symtab.so')
    request = urllib2.Request(url)
    response = urllib2.urlopen(request)
    with open('libflutter.so', "wb") as local_file:
        local_file.write(response.read())
    print('........  Download done !   ........')


def parse_info_and_download_so(hash_code):
    url = BUILD_ID_TOS_URL % hash_code
    request = urllib2.Request(url)
    try:
        response = urllib2.urlopen(request)
    except:
        print('can not find sys so of %s' % hash_code)
        return
    result = response.read().decode('utf-8')
    lines = result.split('\n')
    dict_result = {}
    for line in lines:
        if line.__contains__('='):
            key = line.split('=')[0]
            value = line[len(key) + 1:]
            dict_result[key] = value
    print('---------------------------------------------------------')
    print('|  commitId: %s' % dict_result['cid'])
    print('|  userName: %s' % dict_result['user'])
    print('|  commitMsg: %s' % dict_result['msg'])
    print('|  commitTime: %s' % dict_result['time'])
    print('|  mode : %s' % dict_result['mode'])
    print('---------------------------------------------------------')
    print('')
    download_so(dict_result['cid'], dict_result['mode'])


if __name__ == '__main__':
    if len(sys.argv) != 2:
        print('args len illegal! run this python with hash code value.')
        print('run \' file path_of_libflutter_so to get hash code \'')
    hash_code = sys.argv[1]
    parse_info_and_download_so(hash_code)
