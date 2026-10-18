import unittest

from check_device_link import closure, inspect


class DeviceLink(unittest.TestCase):
    def test_cross_unit_function_and_global(self):
        caller = '.extern .func target();\n.extern .global .align 4 .u32 data;\n.visible .entry kernel() {\n call.uni target, ();\n}\n'
        provider = '.visible .func target() // definition\n{\n ret;\n}\n.visible .global .align 4 .u32 data;\n'
        self.assertEqual(closure({'caller': inspect(caller), 'provider': inspect(provider)}), [])

    def test_missing_call_and_global_fail(self):
        self.assertEqual(len(closure({'caller': inspect(
            '.extern .global .u32 missing;\n.visible .entry kernel() {\n call.uni absent, ();\n}\n')})), 2)

    def test_duplicate_weak_definition_fails(self):
        unit = inspect('.weak .func shared() {\n ret;\n}\n')
        self.assertIn('duplicate shared', closure({'a': unit, 'b': unit})[0])

    def test_local_symbols_and_parameters_are_not_link_references(self):
        unit = inspect('.visible .func _Zf(.param .u32 _Zf_param_0) .noreturn\n{\n'
                       '\t.shared .align 8 .b8 _ZZlocal[8];\n exit;\n}\n')
        self.assertEqual(closure({'a': unit}), [])
        self.assertEqual(unit[2], 1)


if __name__ == '__main__':
    unittest.main()
