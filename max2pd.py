import json, sys
filename = sys.argv[1]

def list2int_string(arr):
    return " ".join(str(int(x)) for x in arr)

def monitor(src, dst):
    print('='*40)
    print(f'{json.dumps(src, indent=4)} \n|\n——————————————————————————————————————> {dst}\n\n')
def obj_line(patch_data):
    
    boxes = patch_data['patcher']['boxes']
    lines = patch_data['patcher']['lines']

    patcher_data = []

    id_to_index = {}

    # map objects order and ID
    for i, item in enumerate(boxes):
        id_to_index[item['box']['id']] = i

    # build objects
    for i in boxes:
        if i['box'].get('patcher'):
            # subpatch info
            main_patch_size = list2int_string(i['box']['patcher']['rect'])
            main_patch_name = i['box']['text'].split(' ')[1]
            main_patch_pos = list2int_string(i['box'].get('patching_rect')[:2])

            # header subpatch
            patcher_data.append(f'#N canvas {main_patch_size} {main_patch_name} 0;')
            
            for src, dst in objs_table.items():
                patcher_data[-1] = patcher_data[-1].replace(src, dst)

            patcher_data.extend(obj_line(i['box']))
            # footer subpatch
            patcher_data.append(f'#X restore {main_patch_pos} p {main_patch_name};')

            for src, dst in objs_table.items():
                patcher_data[-1] = patcher_data[-1].replace(src, dst)

        else:
            
            cls = i['box'].get('maxclass')
            name = i['box'].get('text') or ""
            
            pos = list2int_string(i['box'].get('patching_rect')[:2])
            size = list2int_string(i['box'].get('patching_rect')[2:])
            
            #slider
            min = 0 if i['box'].get('min') == None else int(i['box'].get('min'))
            max = 127 if i['box'].get('size') == None else int(i['box'].get('size'))

            # flonum - integer
            minimum = 0 if i['box'].get('minimum') == None else int(i['box'].get('minimum'))
            maximum = 0 if i['box'].get('maximum') == None else int(i['box'].get('maximum'))
            
            var_name = 'empty' if i['box'].get('varname') == None else i['box'].get('varname')

            bg_color = '#fcfcfc' if i['box'].get('bgcolor') == None else '#' + "".join([f'{int(x*255):02x}' for x in i['box'].get('bgcolor')[:3]])
            fg_color = '#000000' if i['box'].get('elementcolor') == None else '#' + "".join([f'{int(x*255):02x}' for x in i['box'].get('elementcolor')[:3]])
            tx_color = '#000000' if i['box'].get('knobcolor') == None else '#' + "".join([f'{int(x*255):02x}' for x in i['box'].get('knobcolor')[:3]])
            bl_color = '#000000' if i['box'].get('blinkcolor') == None else '#' + "".join([f'{int(x*255):02x}' for x in i['box'].get('blinkcolor')[:3]])
            ln_color = '#000000' if i['box'].get('outlinecolor') == None else '#' + "".join([f'{int(x*255):02x}' for x in i['box'].get('outlinecolor')[:3]])
            ys_color = '#000000' if i['box'].get('checkedcolor') == None else '#' + "".join([f'{int(x*255):02x}' for x in i['box'].get('checkedcolor')[:3]])
            no_color = "#FFFFFF" if i['box'].get('uncheckedcolor') == None else '#' + "".join([f'{int(x*255):02x}' for x in i['box'].get('uncheckedcolor')[:3]])

            blink_time = 250 if i['box'].get('bliktime') == None else i['box'].get('bliktime')

            if cls == 'newobj':
                patcher_data.append(f'#X obj {pos} {name};')
            elif cls == 'flonum' or cls == 'number':
                patcher_data.append(f'#X floatatom {pos} 0 {minimum} {maximum} 0 - {var_name} - 0;')
            elif cls == 'message':
                patcher_data.append(f'#X msg {pos} {name};')                
            elif cls == 'inlet':
                outlettype = i['box'].get('outlettype', [''])[0] if i['box'].get('outlettype') else ''
                suffix = '~' if outlettype == 'signal' else ''
                patcher_data.append(f'#X obj {pos} inlet{suffix};')

            elif cls == 'outlet':

                box_id = i['box']['id']
                is_signal = False
                
                for ln in lines:
                    line = ln['patchline']
                    dst_id, inlet = line['destination']
                    src_id, outlet_idx = line['source']
                    
                    if dst_id == box_id:
                        
                        src_box = next((b['box'] for b in boxes if b['box']['id'] == src_id), None)
                        if src_box:
                            src_outlettype = src_box.get('outlettype', [''])
                            if src_outlettype and len(src_outlettype) > outlet_idx:
                                if src_outlettype[outlet_idx] == 'signal':
                                    is_signal = True
                                    break
                
                suffix = '~' if is_signal else ''
                patcher_data.append(f'#X obj {pos} outlet{suffix};')

            elif cls == 'toggle':
                patcher_data.append(f'#X obj {pos} tgl {size.split(' ')[0]} 0 {var_name} empty empty 0 -9 0 12 {bg_color} {ys_color} {no_color} 0 1;')
            elif cls == 'button':
                patcher_data.append(f'#X obj {pos} bng {size.split(' ')[0]} {blink_time} 50 0 {var_name} empty empty 0 -9 0 12 {bg_color} {bl_color} {ln_color};')
            elif cls == 'slider':
                dir = i['box'].get('orientation')
                if dir == 1:
                    patcher_data.append(f'#X obj {pos} hsl {size} {min} {max} 0 0 {var_name} empty empty 0 -9 0 12 {bg_color} {fg_color} {tx_color} 0 1;')
                elif dir == 0 or dir == 2 or dir == None:
                    patcher_data.append(f'#X obj {pos} vsl {size} {min} {max} 0 0 {var_name} empty empty 0 -9 0 12 {bg_color} {fg_color} {tx_color} 0 1;')


            for src, dst in objs_table.items():
                patcher_data[-1] = patcher_data[-1].replace(src, dst)

            monitor(i, patcher_data[-1])

    # build connections
    for ln in lines:
        line = ln['patchline']

        src_id, outlet = line['source']
        dst_id, inlet = line['destination']

        src_index = id_to_index[src_id]
        dst_index = id_to_index[dst_id]

        patcher_data.append(f'#X connect {src_index} {outlet} {dst_index} {inlet};')

    return patcher_data

def converter(patch):
    # main patch info
    size = list2int_string(patch['patcher']['rect'])
    font_size = str(int(patch['patcher']['default_fontsize']))
    
    main_patch = [f'#N canvas {size} {font_size};']
    main_patch.extend(obj_line(patch))

    return main_patch

# open file .maxpat
with open(filename, 'r', encoding='utf-8') as f:
    patch = json.load(f)
with open('table.json', 'r') as json_file:
    objs_table = json.load(json_file)


fileout = converter(patch)
filename = filename.split('.')[0] + '.pd'
with open(filename, 'w', encoding='utf-8') as f:
    f.write('\n'.join(fileout) + '\n')