const fz = require('zigbee-herdsman-converters/converters/fromZigbee');
const tz = require('zigbee-herdsman-converters/converters/toZigbee');
const exposes = require('zigbee-herdsman-converters/lib/exposes');
const reporting = require('zigbee-herdsman-converters/lib/reporting');
const e = exposes.presets;
const ea = exposes.access;

/* ZCL data type IDs */
const DATA_TYPE = {boolean: 0x10, uint8: 0x20, uint16: 0x21};

/* Custom attribute IDs on the Occupancy Sensing cluster (0x0406) */
const ATTR = {
    range_min:           {id: 0xE000, type: DATA_TYPE.uint16},
    range_max:           {id: 0xE001, type: DATA_TYPE.uint16},
    trigger_range:       {id: 0xE002, type: DATA_TYPE.uint16},
    trigger_sensitivity: {id: 0xE003, type: DATA_TYPE.uint8},
    keep_sensitivity:    {id: 0xE004, type: DATA_TYPE.uint8},
    trigger_delay:       {id: 0xE005, type: DATA_TYPE.uint8},
    keep_timeout:        {id: 0xE006, type: DATA_TYPE.uint16},
    io_polarity:         {id: 0xE007, type: DATA_TYPE.uint8},
    fretting:            {id: 0xE008, type: DATA_TYPE.boolean},
};

const ALL_CUSTOM_IDS = Object.values(ATTR).map((a) => a.id);

const fzLocal = {
    sen0609_config: {
        cluster: 'msOccupancySensing',
        type: ['attributeReport', 'readResponse'],
        convert: (model, msg, publish, options, meta) => {
            const result = {};
            const d = msg.data;
            if (d[0xE000] !== undefined) result.range_min = d[0xE000];
            if (d[0xE001] !== undefined) result.range_max = d[0xE001];
            if (d[0xE002] !== undefined) result.trigger_range = d[0xE002];
            if (d[0xE003] !== undefined) result.trigger_sensitivity = d[0xE003];
            if (d[0xE004] !== undefined) result.keep_sensitivity = d[0xE004];
            if (d[0xE005] !== undefined) result.trigger_delay = d[0xE005];
            if (d[0xE006] !== undefined) result.keep_timeout = d[0xE006];
            if (d[0xE007] !== undefined) result.io_polarity = d[0xE007];
            if (d[0xE008] !== undefined) result.fretting = d[0xE008] ? true : false;
            return result;
        },
    },
};

const tzLocal = {
    sen0609_config: {
        key: Object.keys(ATTR),
        convertSet: async (entity, key, value, meta) => {
            const attr = ATTR[key];
            const writeVal = key === 'fretting' ? (value ? 1 : 0) : value;
            await entity.write('msOccupancySensing', {[attr.id]: {value: writeVal, type: attr.type}});
            return {state: {[key]: value}};
        },
        convertGet: async (entity, key, meta) => {
            await entity.read('msOccupancySensing', [ATTR[key].id]);
        },
    },
};

const definition = {
    /* Match by endpoint/cluster fingerprint.
     * If your device reports a known modelID / manufacturerName in the Basic
     * cluster you can replace this with:
     *   zigbeeModel: ['your_model_id'],
     */
    fingerprint: [{
        type: 'EndDevice',
        endpoints: [{
            ID: 10,
            profileID: 0x0104,
            deviceID: 0x0000,
            inputClusters:  [0x0000, 0x0003, 0x0007, 0x0406],
            outputClusters: [0x0006, 0x0005, 0x0004, 0x0003],
        }],
    }],
    model: 'SEN0609-Zigbee',
    vendor: 'DFRobot',
    description: 'SEN0609 mmWave presence sensor with Zigbee (CC2340)',
    fromZigbee: [fz.occupancy, fz.command_on, fz.command_off, fz.command_toggle, fzLocal.sen0609_config],
    toZigbee: [tzLocal.sen0609_config],
    exposes: [
        e.occupancy(),
        e.action(['on', 'off', 'toggle']),
        e.numeric('range_min', ea.ALL).withUnit('cm')
            .withValueMin(30).withValueMax(2000)
            .withDescription('Minimum detection range'),
        e.numeric('range_max', ea.ALL).withUnit('cm')
            .withValueMin(240).withValueMax(2000)
            .withDescription('Maximum detection range'),
        e.numeric('trigger_range', ea.ALL).withUnit('cm')
            .withValueMin(30).withValueMax(2000)
            .withDescription('Trigger detection range'),
        e.numeric('trigger_sensitivity', ea.ALL)
            .withValueMin(0).withValueMax(9)
            .withDescription('Trigger sensitivity (0=low, 9=high)'),
        e.numeric('keep_sensitivity', ea.ALL)
            .withValueMin(0).withValueMax(9)
            .withDescription('Keep sensitivity (0=low, 9=high)'),
        e.numeric('trigger_delay', ea.ALL)
            .withValueMin(0).withValueMax(200)
            .withDescription('Trigger delay (unit: 10 ms, range 0-2 s)'),
        e.numeric('keep_timeout', ea.ALL)
            .withValueMin(4).withValueMax(3000)
            .withDescription('Keep timeout (unit: 500 ms, range 2-1500 s)'),
        e.binary('io_polarity', ea.ALL, 1, 0)
            .withDescription('Output pin polarity'),
        e.binary('fretting', ea.ALL, true, false)
            .withDescription('Micromotion (fretting) detection'),
    ],
    configure: async (device, coordinatorEndpoint, definition) => {
        const endpoint = device.getEndpoint(10);
        await reporting.bind(endpoint, coordinatorEndpoint, ['msOccupancySensing']);
        await endpoint.configureReporting('msOccupancySensing', [{
            attribute: 'occupancy',
            minimumReportInterval: 0,
            maximumReportInterval: 300,
            reportableChange: 1,
        }]);
        /* Read initial sensor configuration */
        await endpoint.read('msOccupancySensing', ALL_CUSTOM_IDS);
    },
};

module.exports = definition;
