from pupylib.PupyModule import config, PupyModule, PupyArgumentParser
from pupylib.PupyCompleter import remote_path_completer
from pupylib.PupyOutput import Table, Line, List, MultiPart
from modules.lib import size_human_readable, file_timestamp
from M2Crypto.X509 import load_cert_string, FORMAT_DER
from argparse import REMAINDER

from magic import Magic

__class_name__="FStat"

@config(cat='admin', compat=['windows', 'linux'])
class FStat(PupyModule):
    '''Show a bit more info about file path. ACLs/Caps/Owner for now'''

    dependencies = {
        'all': [
            'pupyutils.basic_cmds', 'fsutils', 'fsutils_ext'
        ],
        'windows': ['junctions', 'ntfs_streams', 'pupwinutils.security'],
        'linux': ['xattr', 'posix1e', 'prctl', '_prctl']
    }

    @classmethod
    def init_argparse(cls):
        cls.arg_parser = PupyArgumentParser(prog='stat', description=cls.__doc__)
        cls.arg_parser.add_argument(
            '-v', '--verbose', action='store_true', default=False,
            help='Print more information (certificates for example)'
        )
        cls.arg_parser.add_argument(
            'path', type=str, nargs=REMAINDER,
            help='path of a specific file', completer=remote_path_completer)

    def run(self, args):
        getfilesec = self.client.remote('fsutils_ext', 'getfilesec')

        path = ' '.join(args.path)

        try:
            sec = getfilesec(path)
        except Exception, e:
            self.error(' '.join(x for x in e.args if type(x) in (str, unicode)))
            return

        ctime, atime, mtime, size, owner, group, header, mode, extra = sec

        owner_id, owner_name, owner_domain = owner
        group_id, group_name, group_domain = group

        default = {
            'Created': file_timestamp(ctime, time=True),
            'Accessed': file_timestamp(atime, time=True),
            'Modified': file_timestamp(mtime, time=True),
            'Size': '{} ({})'.format(size_human_readable(size), size),
            'Owner': '{}{} ({})'.format(
                owner_domain+'\\' if owner_domain else '',
                owner_name,
                owner_id
            ),
            'Group': '{}{} ({})'.format(
                group_domain+'\\' if group_domain else '',
                group_name,
                group_id
            ),
            'Mode': mode,
        }

        infos = []

        infos.append(Table([
            {'Property': p, 'Value': default[p]} for p in (
                'Created', 'Accessed', 'Modified',
                'Size', 'Owner', 'Group', 'Mode'
            )
        ], ['Property', 'Value'], legend=False))

        oneliners = []

        certificates = None

        for extra, values in extra.iteritems():
            if extra == 'Certificates':
                certificates = [
                    load_cert_string(cert, FORMAT_DER).as_text() for cert in values
                ]
            elif isinstance(values, dict):
                records = [{
                    'KEY': k.decode('utf-8'),
                    'VALUE': v.decode('utf-8') if isinstance(v, str) else str(v)
                } for k, v in values.iteritems()]

                infos.append(
                    Table(records, ['KEY', 'VALUE'], caption=extra)
                )
            elif isinstance(values, (list, tuple)):
                if all(isinstance(
                        value, (list, tuple)) and len(value) == 2 for value in values):
                    infos.append(List(
                        '{}: {}'.format(key, value) for key, value in values
                    ))
                else:
                    infos.append(List(values, caption=extra))
            elif isinstance(values, int):
                oneliners.append('{}: {}'.format(extra, values))
            elif '\n' in values:
                infos.append(Line(extra+':', values))
            else:
                oneliners.append(extra+': ' + values)

        if args.verbose:
            magic = ''
            if header:
                with Magic() as libmagic:
                    magic = libmagic.id_buffer(header)

            if magic:
                oneliners.append('Magic: {}'.format(magic))

            if certificates:
                infos.extend(certificates)

        if oneliners:
            infos.append(List(oneliners, caption='Other'))

        self.log(MultiPart(infos))
