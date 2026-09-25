"""Validate the generated native popup without starting Mewgenics."""
import sys
import struct
import subprocess
import zlib
import unittest
from pathlib import Path

sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from build_steve_prompt import build,read_base,placement,write_game_asset,NODE,ROOT
from swf import read_tags,tag_start,symbol_entries,DEFINITION_TAGS
from swf import Reader,point
from abc_patch import AbcModule


class StevePromptTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.base=read_base(); cls.changed,cls.info=build(cls.base)
        cls.old=list(read_tags(cls.base,tag_start(cls.base)))
        cls.new=list(read_tags(cls.changed,tag_start(cls.changed)))

    def test_other_assets_preserved(self):
        def ordinary(entries):
            return [raw for kind,body,raw in entries if kind not in (76,82) and
                    not (kind==39 and struct.unpack_from('<H',body)[0] in (self.info['button'],self.info['prompt']))]
        self.assertEqual(ordinary(self.old),ordinary(self.new))
        self.assertEqual(struct.unpack_from('<I',self.changed,4)[0],len(self.changed))

    def test_actual_cli_output_is_directly_readable_by_game(self):
        target=ROOT/'build'/'test_steve_prompt_output.swf'
        subprocess.run([sys.executable,'-B',str(ROOT/'tools'/'build_steve_prompt.py'),'--output',str(target)],check=True,capture_output=True)
        actual=target.read_bytes()
        # Deliberately do not decompress here: the game does not either.
        self.assertEqual(actual[:3],b'FWS')
        self.assertEqual(struct.unpack_from('<I',actual,4)[0],len(actual))
        self.assertEqual(actual,self.changed)
        parsed=list(read_tags(actual,tag_start(actual)))
        self.assertEqual(parsed[-1][0],0)
        self.assertEqual(sum(kind==82 for kind,_,_ in parsed),1)

    def test_writer_rejects_the_crashing_compressed_format(self):
        target=ROOT/'build'/'test_steve_prompt_reject.swf'
        sentinel=b'unchanged'; target.write_bytes(sentinel)
        compressed=b'CWS'+self.changed[3:8]+zlib.compress(self.changed[8:])
        with self.assertRaises(ValueError): write_game_asset(compressed,target)
        self.assertEqual(target.read_bytes(),sentinel)
        with self.assertRaises(ValueError): write_game_asset(self.changed[:-1],target)
        self.assertEqual(target.read_bytes(),sentinel)

    def test_single_abc_and_original_records(self):
        blocks=[body for kind,body,_ in self.new if kind==82]
        self.assertEqual(len(blocks),1)
        old=next(body for kind,body,_ in self.old if kind==82)
        a=AbcModule(old[old.index(0,4)+1:]); b=AbcModule(blocks[0][blocks[0].index(0,4)+1:])
        for key,table in a.tables.items(): self.assertEqual(table.records,b.tables[key].records[:len(table.records)])
        old_symbols=[entry for kind,body,_ in self.old if kind==76 for entry in symbol_entries(body)]
        new_symbols=[entry for kind,body,_ in self.new if kind==76 for entry in symbol_entries(body)]
        self.assertEqual(new_symbols,old_symbols+[(self.info['button'],self.info['class'])])

    def test_prompt_timeline_and_extra_row(self):
        def clip(entries,ident): return next(body for kind,body,_ in entries if kind==39 and struct.unpack_from('<H',body)[0]==ident)
        old=clip(self.old,self.info['prompt']); new=clip(self.new,self.info['prompt'])
        entries=list(read_tags(new,4)); kept=[]; extra=[]
        for kind,body,raw in entries:
            if kind==26 and placement(body)[1]==self.info['depth']: extra.append(placement(body))
            elif kind==28 and struct.unpack_from('<H',body)[0]==self.info['depth']: pass
            else: kept.append(raw)
        self.assertEqual(old,new[:4]+b''.join(kept))
        self.assertEqual(sum(p[5]==NODE for p in extra),1)
        self.assertEqual(sum(kind==1 for kind,_,_ in entries),struct.unpack_from('<H',old,2)[0])
        # The original Yes/No controls and the new control disappear together.
        first_removed=next(i for i,(kind,body,_) in enumerate(entries) if kind==28 and struct.unpack_from('<H',body)[0]==5)
        self.assertTrue(any(kind==28 and struct.unpack_from('<H',body)[0]==self.info['depth'] for kind,body,_ in entries[first_removed:first_removed+5]))
        placements=[placement(body) for kind,body,_ in entries if kind==26]
        named={p[5]:p for p in placements if p[5]}
        self.assertEqual(named[NODE][3][5]-named['yes'][3][5],2800)
        self.assertEqual(named[NODE][3][4],round((named['yes'][3][4]+named['no'][3][4])/2))

    def test_visible_button_does_not_overlap_yes_no(self):
        defs={struct.unpack_from('<H',body)[0]:(kind,body) for kind,body,_ in self.new if kind in DEFINITION_TAGS}
        def bounds(ident):
            kind,body=defs[ident]
            if kind in (2,22,32,37,83):
                r=Reader(body,2); n=r.u(5); x0,x1,y0,y1=[r.s(n) for _ in range(4)]
                return [(x0,y0),(x0,y1),(x1,y0),(x1,y1)]
            if kind!=39: return []
            out=[]
            for code,data,_ in read_tags(body,4):
                if code==1: break
                if code==26:
                    p=placement(data)
                    if p[2]: out.extend(point(p[3] or (1,0,0,1,0,0),v) for v in bounds(p[2]))
            return out
        frame=0; nodes={}
        for kind,body,_ in read_tags(defs[self.info['prompt']][1],4):
            if kind==26:
                _,depth,child,transform,_,name=placement(body)
                node=nodes.setdefault(depth,{})
                if child is not None: node['child']=child
                if transform: node['transform']=transform
                if name: node['name']=name
            if kind==1:
                if frame==9:
                    controls={v['name']:v for v in nodes.values() if v.get('name') in ('yes','no',NODE)}
                    shape=bounds(controls['yes']['child'])
                    pts={name:[point(v['transform'],p) for p in shape] for name,v in controls.items()}
                    self.assertGreater(min(p[1] for p in pts[NODE]),max(p[1] for p in pts['yes']+pts['no'])+8*20)
                    self.assertLess(max(p[1] for p in pts[NODE]),720*20)
                    return
                frame+=1
        self.fail('Stable prompt frame missing')

    def test_button_hidden_until_bound(self):
        body=next(body for kind,body,_ in self.new if kind==39 and struct.unpack_from('<H',body)[0]==self.info['button'])
        frames=[[]]
        for kind,data,_ in read_tags(body,4):
            if kind==1: frames.append([])
            elif kind!=0: frames[-1].append((kind,data))
        self.assertEqual(struct.unpack_from('<H',body,2)[0],7)
        self.assertFalse(any(kind==26 for kind,_ in frames[0]))
        names=[next(data.split(b'\0')[0] for kind,data in frame if kind==43) for frame in frames[:-1]]
        self.assertEqual(names,[b'hidden',b'up',b'over',b'down',b'selected',b'disabled',b'enable'])
        for frame in frames[1:-1]: self.assertTrue(any(kind==26 for kind,_ in frame))


if __name__=='__main__': unittest.main()
