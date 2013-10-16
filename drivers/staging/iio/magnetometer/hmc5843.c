/*  Copyright (C) 2010 Texas Instruments
    Author: Shubhrajyoti Datta <shubhrajyoti@ti.com>
    Acknowledgement: Jonathan Cameron <jic23@kernel.org> for valuable inputs.

    Support for HMC5883 and HMC5883L by Peter Meerwald <pmeerw@pmeerw.net>.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/iio/iio.h>
#include <linux/iio/sysfs.h>
#include <linux/delay.h>

#define HMC5843_CONFIG_REG_A			0x00
#define HMC5843_CONFIG_REG_B			0x01
#define HMC5843_MODE_REG			0x02
#define HMC5843_DATA_OUT_X_MSB_REG		0x03
#define HMC5843_DATA_OUT_Y_MSB_REG		0x05
#define HMC5843_DATA_OUT_Z_MSB_REG		0x07
/* Beware: Y and Z are exchanged on HMC5883 */
#define HMC5883_DATA_OUT_Z_MSB_REG		0x05
#define HMC5883_DATA_OUT_Y_MSB_REG		0x07
#define HMC5843_STATUS_REG			0x09

enum hmc5843_ids {
	HMC5843_ID,
	HMC5883_ID,
	HMC5883L_ID,
};

/*
 * Range gain settings in (+-)Ga
 * Beware: HMC5843 and HMC5883 have different recommended sensor field
 * ranges; default corresponds to +-1.0 Ga and +-1.3 Ga, respectively
 */
#define HMC5843_RANGE_GAIN_OFFSET		0x05
#define HMC5843_RANGE_GAIN_DEFAULT		0x01
#define HMC5843_RANGE_GAINS			8

/* Device status */
#define HMC5843_DATA_READY			0x01
#define HMC5843_DATA_OUTPUT_LOCK		0x02

/* Mode register configuration */
#define HMC5843_MODE_CONVERSION_CONTINUOUS	0x00
#define HMC5843_MODE_CONVERSION_SINGLE		0x01
#define HMC5843_MODE_IDLE			0x02
#define HMC5843_MODE_SLEEP			0x03
#define HMC5843_MODE_MASK			0x03

/*
 * HMC5843: Minimum data output rate
 * HMC5883: Typical data output rate
 */
#define HMC5843_RATE_OFFSET			0x02
#define HMC5843_RATE_DEFAULT			0x04
#define HMC5843_RATE_BITMASK			0x1C
#define HMC5843_RATE_NOT_USED			0x07

/* Device measurement configuration */
#define HMC5843_MEAS_CONF_NORMAL		0x00
#define HMC5843_MEAS_CONF_POSITIVE_BIAS		0x01
#define HMC5843_MEAS_CONF_NEGATIVE_BIAS		0x02
#define HMC5843_MEAS_CONF_NOT_USED		0x03
#define HMC5843_MEAS_CONF_MASK			0x03

/* Scaling factors: 10000000/Gain */
static const int hmc5843_regval_to_nanoscale[HMC5843_RANGE_GAINS] = {
	6173, 7692, 10309, 12821, 18868, 21739, 25641, 35714
};

static const int hmc5883_regval_to_nanoscale[HMC5843_RANGE_GAINS] = {
	7812, 9766, 13021, 16287, 24096, 27701, 32573, 45662
};

static const int hmc5883l_regval_to_nanoscale[HMC5843_RANGE_GAINS] = {
	7299, 9174, 12195, 15152, 22727, 25641, 30303, 43478
};

/*
 * From the datasheet:
 * Value	| HMC5843		| HMC5883/HMC5883L
 *		| Data output rate (Hz)	| Data output rate (Hz)
 * 0		| 0.5			| 0.75
 * 1		| 1			| 1.5
 * 2		| 2			| 3
 * 3		| 5			| 7.5
 * 4		| 10 (default)		| 15
 * 5		| 20			| 30
 * 6		| 50			| 75
 * 7		| Not used		| Not used
 */
static const int hmc5843_regval_to_samp_freq[7][2] = {
	{0, 500000}, {1, 0}, {2, 0}, {5, 0}, {10, 0}, {20, 0}, {50, 0}
};

static const int hmc5883_regval_to_samp_freq[7][2] = {
	{0, 750000}, {1, 500000}, {3, 0}, {7, 500000}, {15, 0}, {30, 0},
	{75, 0}
};

/* Describe chip variants */
struct hmc5843_chip_info {
	const struct iio_chan_spec *channels;
	const int (*regval_to_samp_freq)[2];
	const int *regval_to_nanoscale;
};

/* Each client has this additional data */
struct hmc5843_data {
	struct i2c_client *client;
	struct mutex lock;
	u8 rate;
	u8 meas_conf;
	u8 operating_mode;
	u8 range;
	const struct hmc5843_chip_info *variant;
};

/* The lower two bits contain the current conversion mode */
static s32 hmc5843_configure(struct i2c_client *client,
				       u8 operating_mode)
{
	return i2c_smbus_write_byte_data(client,
					HMC5843_MODE_REG,
					operating_mode & HMC5843_MODE_MASK);
}

/* Return the measurement value from the specified channel */
static int hmc5843_read_measurement(struct hmc5843_data *data,
				    int address, int *val)
{
	s32 result;
	int tries = 150;

	mutex_lock(&data->lock);
	while (tries-- > 0) {
		result = i2c_smbus_read_byte_data(data->client,
			HMC5843_STATUS_REG);
		if (result & HMC5843_DATA_READY)
			break;
		msleep(20);
	}

	if (tries < 0) {
		dev_err(&data->client->dev, "data not ready\n");
		mutex_unlock(&data->lock);
		return -EIO;
	}

	result = i2c_smbus_read_word_swapped(data->client, address);
	mutex_unlock(&data->lock);
	if (result < 0)
		return -EINVAL;

	*val = sign_extend32(result, 15);
	return IIO_VAL_INT;
}

/*
 * From the datasheet:
 * 0 - Continuous-Conversion Mode: In continuous-conversion mode, the
 *     device continuously performs conversions and places the result in
 *     the data register.
 *
 * 1 - Single-Conversion Mode : Device performs a single measurement,
 *     sets RDY high and returns to sleep mode.
 *
 * 2 - Idle Mode : Device is placed in idle mode.
 *
 * 3 - Sleep Mode : Device is placed in sleep mode.
 *
 */
static ssize_t hmc5843_show_operating_mode(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	struct iio_dev *indio_dev = dev_to_iio_dev(dev);
	struct hmc5843_data *data = iio_priv(indio_dev);
	return sprintf(buf, "%d\n", data->operating_mode);
}

static ssize_t hmc5843_set_operating_mode(struct device *dev,
				struct device_attribute *attr,
				const char *buf,
				size_t count)
{
	struct iio_dev *indio_dev = dev_to_iio_dev(dev);
	struct i2c_client *client = to_i2c_client(indio_dev->dev.parent);
	struct hmc5843_data *data = iio_priv(indio_dev);
	struct iio_dev_attr *this_attr = to_iio_dev_attr(attr);
	unsigned long operating_mode = 0;
	s32 status;
	int error;

	mutex_lock(&data->lock);
	error = kstrtoul(buf, 10, &operating_mode);
	if (error) {
		count = error;
		goto exit;
	}
	dev_dbg(dev, "set conversion mode to %lu\n", operating_mode);
	if (operating_mode > HMC5843_MODE_SLEEP) {
		count = -EINVAL;
		goto exit;
	}

	status = i2c_smbus_write_byte_data(client, this_attr->address,
					operating_mode);
	if (status) {
		count = -EINVAL;
		goto exit;
	}
	data->operating_mode = operating_mode;

exit:
	mutex_unlock(&data->lock);
	return count;
}

static IIO_DEVICE_ATTR(operating_mode,
			S_IWUSR | S_IRUGO,
			hmc5843_show_operating_mode,
			hmc5843_set_operating_mode,
			HMC5843_MODE_REG);

/*
 * API for setting the measurement configuration to
 * Normal, Positive bias and Negative bias
 *
 * From the datasheet:
 * 0 - Normal measurement configuration (default): In normal measurement
 *     configuration the device follows normal measurement flow. Pins BP
 *     and BN are left floating and high impedance.
 *
 * 1 - Positive bias configuration: In positive bias configuration, a
 *     positive current is forced across the resistive load on pins BP
 *     and BN.
 *
 * 2 - Negative bias configuration. In negative bias configuration, a
 *     negative current is forced across the resistive load on pins BP
 *     and BN.
 *
 */
static s32 hmc5843_set_meas_conf(struct hmc5843_data *data, u8 meas_conf)
{
	u8 reg_val;
	reg_val = (meas_conf & HMC5843_MEAS_CONF_MASK) |
		(data->rate << HMC5843_RATE_OFFSET);
	return i2c_smbus_write_byte_data(data->client, HMC5843_CONFIG_REG_A,
		reg_val);
}

static ssize_t hmc5843_show_measurement_configuration(struct device *dev,
						struct device_attribute *attr,
						char *buf)
{
	struct iio_dev *indio_dev = dev_to_iio_dev(dev);
	struct hmc5843_data *data = iio_priv(indio_dev);
	return sprintf(buf, "%d\n", data->meas_conf);
}

static ssize_t hmc5843_set_measurement_configuration(struct device *dev,
						struct device_attribute *attr,
						const char *buf,
						size_t count)
{
	struct iio_dev *indio_dev = dev_to_iio_dev(dev);
	struct hmc5843_data *data = iio_priv(indio_dev);
	unsigned long meas_conf = 0;
	int error;

	error = kstrtoul(buf, 10, &meas_conf);
	if (error)
		return error;
	if (meas_conf >= HMC5843_MEAS_CONF_NOT_USED)
		return -EINVAL;

	mutex_lock(&data->lock);
	dev_dbg(dev, "set measurement configuration to %lu\n", meas_conf);
	if (hmc5843_set_meas_conf(data, meas_conf)) {
		count = -EINVAL;
		goto exit;
	}
	data->meas_conf = meas_conf;

exit:
	mutex_unlock(&data->lock);
	return count;
}

static IIO_DEVICE_ATTR(meas_conf,
			S_IWUSR | S_IRUGO,
			hmc5843_show_measurement_configuration,
			hmc5843_set_measurement_configuration,
			0);

static ssize_t hmc5843_show_int_plus_micros(char *buf,
	const int (*vals)[2], int n)
{
	size_t len = 0;
	int i;

	for (i = 0; i < n; i++)
		len += scnprintf(buf + len, PAGE_SIZE - len,
			"%d.%d ", vals[i][0], vals[i][1]);

	/* replace trailing space by newline */
	buf[len - 1] = '\n';

	return len;
}

static int hmc5843_check_int_plus_micros(const int (*vals)[2], int n,
					int val, int val2)
{
	int i;

	for (i = 0; i < n; i++) {
		if (val == vals[i][0] && val2 == vals[i][1])
			return i;
	}

	return -EINVAL;
}

static ssize_t hmc5843_show_samp_freq_avail(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct hmc5843_data *data = iio_priv(dev_to_iio_dev(dev));

	return hmc5843_show_int_plus_micros(buf,
		data->variant->regval_to_samp_freq, HMC5843_RATE_NOT_USED);
}

static IIO_DEV_ATTR_SAMP_FREQ_AVAIL(hmc5843_show_samp_freq_avail);

static s32 hmc5843_set_rate(struct hmc5843_data *data, u8 rate)
{
	u8 reg_val = data->meas_conf | (rate << HMC5843_RATE_OFFSET);

	return i2c_smbus_write_byte_data(data->client, HMC5843_CONFIG_REG_A,
		reg_val);
}

static int hmc5843_check_samp_freq(struct hmc5843_data *data,
				   int val, int val2)
{
	return hmc5843_check_int_plus_micros(
		data->variant->regval_to_samp_freq, HMC5843_RATE_NOT_USED,
		val, val2);
}

static ssize_t hmc5843_show_scale_avail(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct hmc5843_data *data = iio_priv(dev_to_iio_dev(dev));

	size_t len = 0;
	int i;

	for (i = 0; i < HMC5843_RANGE_GAINS; i++)
		len += scnprintf(buf + len, PAGE_SIZE - len,
			"0.%09d ", data->variant->regval_to_nanoscale[i]);

	/* replace trailing space by newline */
	buf[len - 1] = '\n';

	return len;
}

static IIO_DEVICE_ATTR(scale_available, S_IRUGO,
	hmc5843_show_scale_avail, NULL, 0);

static int hmc5843_get_scale_index(struct hmc5843_data *data, int val, int val2)
{
	int i;

	if (val != 0)
		return -EINVAL;

	for (i = 0; i < HMC5843_RANGE_GAINS; i++)
		if (val2 == data->variant->regval_to_nanoscale[i])
			return i;

	return -EINVAL;
}

static int hmc5843_read_raw(struct iio_dev *indio_dev,
			    struct iio_chan_spec const *chan,
			    int *val, int *val2, long mask)
{
	struct hmc5843_data *data = iio_priv(indio_dev);

	switch (mask) {
	case IIO_CHAN_INFO_RAW:
		return hmc5843_read_measurement(data, chan->address, val);
	case IIO_CHAN_INFO_SCALE:
		*val = 0;
		*val2 = data->variant->regval_to_nanoscale[data->range];
		return IIO_VAL_INT_PLUS_NANO;
	case IIO_CHAN_INFO_SAMP_FREQ:
		*val = data->variant->regval_to_samp_freq[data->rate][0];
		*val2 = data->variant->regval_to_samp_freq[data->rate][1];
		return IIO_VAL_INT_PLUS_MICRO;
	}
	return -EINVAL;
}

static int hmc5843_write_raw(struct iio_dev *indio_dev,
			     struct iio_chan_spec const *chan,
			     int val, int val2, long mask)
{
	struct hmc5843_data *data = iio_priv(indio_dev);
	int ret, rate, range;

	switch (mask) {
	case IIO_CHAN_INFO_SAMP_FREQ:
		rate = hmc5843_check_samp_freq(data, val, val2);
		if (rate < 0)
			return -EINVAL;

		mutex_lock(&data->lock);
		ret = hmc5843_set_rate(data, rate);
		if (ret >= 0)
			data->rate = rate;
		mutex_unlock(&data->lock);

		return ret;
	case IIO_CHAN_INFO_SCALE:
		range = hmc5843_get_scale_index(data, val, val2);
		if (range < 0)
			return -EINVAL;

		range <<= HMC5843_RANGE_GAIN_OFFSET;
		mutex_lock(&data->lock);
		ret = i2c_smbus_write_byte_data(data->client,
			HMC5843_CONFIG_REG_B, range);
		if (ret >= 0)
			data->range = range;
		mutex_unlock(&data->lock);

		return ret;
	default:
		return -EINVAL;
	}
}

static int hmc5843_write_raw_get_fmt(struct iio_dev *indio_dev,
			       struct iio_chan_spec const *chan, long mask)
{
	switch (mask) {
	case IIO_CHAN_INFO_SAMP_FREQ:
		return IIO_VAL_INT_PLUS_MICRO;
	case IIO_CHAN_INFO_SCALE:
		return IIO_VAL_INT_PLUS_NANO;
	default:
		return -EINVAL;
	}
}

#define HMC5843_CHANNEL(axis, addr)					\
	{								\
		.type = IIO_MAGN,					\
		.modified = 1,						\
		.channel2 = IIO_MOD_##axis,				\
		.info_mask_separate = BIT(IIO_CHAN_INFO_RAW),		\
		.info_mask_shared_by_type = BIT(IIO_CHAN_INFO_SCALE) |	\
			BIT(IIO_CHAN_INFO_SAMP_FREQ),			\
		.address = addr						\
	}

static const struct iio_chan_spec hmc5843_channels[] = {
	HMC5843_CHANNEL(X, HMC5843_DATA_OUT_X_MSB_REG),
	HMC5843_CHANNEL(Y, HMC5843_DATA_OUT_Y_MSB_REG),
	HMC5843_CHANNEL(Z, HMC5843_DATA_OUT_Z_MSB_REG),
};

static const struct iio_chan_spec hmc5883_channels[] = {
	HMC5843_CHANNEL(X, HMC5843_DATA_OUT_X_MSB_REG),
	HMC5843_CHANNEL(Y, HMC5883_DATA_OUT_Y_MSB_REG),
	HMC5843_CHANNEL(Z, HMC5883_DATA_OUT_Z_MSB_REG),
};

static struct attribute *hmc5843_attributes[] = {
	&iio_dev_attr_meas_conf.dev_attr.attr,
	&iio_dev_attr_operating_mode.dev_attr.attr,
	&iio_dev_attr_scale_available.dev_attr.attr,
	&iio_dev_attr_sampling_frequency_available.dev_attr.attr,
	NULL
};

static const struct attribute_group hmc5843_group = {
	.attrs = hmc5843_attributes,
};

static const struct hmc5843_chip_info hmc5843_chip_info_tbl[] = {
	[HMC5843_ID] = {
		.channels = hmc5843_channels,
		.regval_to_samp_freq = hmc5843_regval_to_samp_freq,
		.regval_to_nanoscale = hmc5843_regval_to_nanoscale,
	},
	[HMC5883_ID] = {
		.channels = hmc5883_channels,
		.regval_to_samp_freq = hmc5883_regval_to_samp_freq,
		.regval_to_nanoscale = hmc5883_regval_to_nanoscale,
	},
	[HMC5883L_ID] = {
		.channels = hmc5883_channels,
		.regval_to_samp_freq = hmc5883_regval_to_samp_freq,
		.regval_to_nanoscale = hmc5883l_regval_to_nanoscale,
	},
};

static void hmc5843_init(struct hmc5843_data *data)
{
	hmc5843_set_meas_conf(data, HMC5843_MEAS_CONF_NORMAL);
	hmc5843_set_rate(data, HMC5843_RATE_DEFAULT);
	hmc5843_configure(data->client, HMC5843_MODE_CONVERSION_CONTINUOUS);
	i2c_smbus_write_byte_data(data->client, HMC5843_CONFIG_REG_B,
		HMC5843_RANGE_GAIN_DEFAULT);
}

static const struct iio_info hmc5843_info = {
	.attrs = &hmc5843_group,
	.read_raw = &hmc5843_read_raw,
	.write_raw = &hmc5843_write_raw,
	.write_raw_get_fmt = &hmc5843_write_raw_get_fmt,
	.driver_module = THIS_MODULE,
};

static int hmc5843_probe(struct i2c_client *client,
			 const struct i2c_device_id *id)
{
	struct hmc5843_data *data;
	struct iio_dev *indio_dev;
	int err = 0;

	indio_dev = devm_iio_device_alloc(&client->dev, sizeof(*data));
	if (indio_dev == NULL)
		return -ENOMEM;

	/* default settings at probe */
	data = iio_priv(indio_dev);
	data->client = client;
	data->variant = &hmc5843_chip_info_tbl[id->driver_data];
	mutex_init(&data->lock);

	i2c_set_clientdata(client, indio_dev);
	indio_dev->info = &hmc5843_info;
	indio_dev->name = id->name;
	indio_dev->dev.parent = &client->dev;
	indio_dev->modes = INDIO_DIRECT_MODE;
	indio_dev->channels = data->variant->channels;
	indio_dev->num_channels = 3;

	hmc5843_init(data);

	err = iio_device_register(indio_dev);
	if (err)
		return err;

	return 0;
}

static int hmc5843_remove(struct i2c_client *client)
{
	iio_device_unregister(i2c_get_clientdata(client));
	 /*  sleep mode to save power */
	hmc5843_configure(client, HMC5843_MODE_SLEEP);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int hmc5843_suspend(struct device *dev)
{
	hmc5843_configure(to_i2c_client(dev), HMC5843_MODE_SLEEP);

	return 0;
}

static int hmc5843_resume(struct device *dev)
{
	struct hmc5843_data *data = iio_priv(i2c_get_clientdata(
		to_i2c_client(dev)));

	hmc5843_configure(data->client, data->operating_mode);

	return 0;
}

static SIMPLE_DEV_PM_OPS(hmc5843_pm_ops, hmc5843_suspend, hmc5843_resume);
#define HMC5843_PM_OPS (&hmc5843_pm_ops)
#else
#define HMC5843_PM_OPS NULL
#endif

static const struct i2c_device_id hmc5843_id[] = {
	{ "hmc5843", HMC5843_ID },
	{ "hmc5883", HMC5883_ID },
	{ "hmc5883l", HMC5883L_ID },
	{ }
};
MODULE_DEVICE_TABLE(i2c, hmc5843_id);

static struct i2c_driver hmc5843_driver = {
	.driver = {
		.name	= "hmc5843",
		.pm	= HMC5843_PM_OPS,
	},
	.id_table	= hmc5843_id,
	.probe		= hmc5843_probe,
	.remove		= hmc5843_remove,
};
module_i2c_driver(hmc5843_driver);

MODULE_AUTHOR("Shubhrajyoti Datta <shubhrajyoti@ti.com>");
MODULE_DESCRIPTION("HMC5843/5883/5883L driver");
MODULE_LICENSE("GPL");
