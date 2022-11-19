﻿using dnlib.DotNet;
using dnlib.DotNet.Emit;
using System.Collections.Generic;
using System.Linq;

namespace dnlib.test
{
    public class NumObfus
    {
        public ModuleDef Module;
        public Dictionary<int, FieldDef> Numbers;
        public NumObfus(ModuleDefMD moduleDef)
        {
            Module = moduleDef;
            Numbers = new Dictionary<int, FieldDef>();
        }
        public void Execute()
        {
            foreach (var type in Module.Types.Where(x => x != Module.GlobalType))
                foreach (var method in type.Methods.Where(x => !x.IsConstructor && x.HasBody && x.Body.HasInstructions))
                    ObfusMethod(method);
        }
        public FieldDef AddNumberField(int num)
        {
            var cstype = Tools.GetRuntimeType("dnlib.test.ModuleType.Num2Modle");
            FieldDef field = cstype.FindField("NUM");
            NameGenerator.GetObfusName(field, NameGenerator.Mode.Base64, 2);
            field.DeclaringType = null;
            Module.GlobalType.Fields.Add(field);

            var method = Module.GlobalType.FindOrCreateStaticConstructor();
            method.Body.Instructions.Insert(0, new Instruction(OpCodes.Ldc_I4, num));
            method.Body.Instructions.Insert(1, new Instruction(OpCodes.Stsfld, field));
            return field;
        }
        public void ObfusMethod(MethodDef method)
        {
            for (int i = 0; i < method.Body.Instructions.Count; i++)
            {
                var instr = method.Body.Instructions[i];
                if (instr.IsLdcI4())
                {
                    if (Module.GlobalType.Fields.Count < 65000)
                    {
                        var Value = instr.GetLdcI4Value();
                        if (Value == 0 || Value == 1)
                            continue;
                        FieldDef fld;
                        if (!Numbers.TryGetValue(Value, out fld))
                        {
                            fld = AddNumberField(Value);
                            Numbers.Add(Value, fld);
                        }
                        instr.OpCode = OpCodes.Ldsfld;
                        instr.Operand = fld;
                    }
                }
            }
        }
    }
}