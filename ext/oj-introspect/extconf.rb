require "mkmf"
require "pathname"
require "oj"

oj_version_file = Oj.const_source_location(:VERSION).first
oj_version_file_path = Pathname.new(oj_version_file)

OJ_HEADERS = oj_version_file_path.join('..', '..', '..', 'ext', 'oj').to_s

dir_config('oj', [OJ_HEADERS], [])

create_makefile("oj-introspect/introspect_ext")
