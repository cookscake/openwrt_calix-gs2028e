// SPDX-License-Identifier: GPL-2.0
/*
 * Calix GS2028E PHY Reset Platform Driver
 *
 * Performs a hard reset of the QCA8075 PHY package using two GPIOs
 * BEFORE the SSDK driver loads, to undo the bad state left by bootloader.
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/gpio/consumer.h>
#include <linux/delay.h>
#include <linux/of.h>

struct calix_phy_reset {
	struct gpio_desc *gpio75;
	struct gpio_desc *gpio77;
};

static int calix_phy_reset_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct calix_phy_reset *reset;
	int ret;

	dev_info(dev, "Starting PHY reset sequence\n");

	reset = devm_kzalloc(dev, sizeof(*reset), GFP_KERNEL);
	if (!reset)
		return -ENOMEM;

	/* Get GPIO 75 (first in phy-reset-gpios array) */
	reset->gpio75 = devm_gpiod_get_index(dev, "phy-reset", 0, GPIOD_OUT_HIGH);
	if (IS_ERR(reset->gpio75)) {
		ret = PTR_ERR(reset->gpio75);
		dev_err(dev, "Failed to get GPIO 75: %d\n", ret);
		return ret;
	}

	/* Get GPIO 77 (second in phy-reset-gpios array) */
	reset->gpio77 = devm_gpiod_get_index(dev, "phy-reset", 1, GPIOD_OUT_HIGH);
	if (IS_ERR(reset->gpio77)) {
		ret = PTR_ERR(reset->gpio77);
		dev_err(dev, "Failed to get GPIO 77: %d\n", ret);
		return ret;
	}

	/* Both GPIOs are now HIGH (reset asserted) */
	dev_info(dev, "PHY reset asserted (GPIO75=HIGH, GPIO77=HIGH)\n");

	/* Hold reset for 50ms */
	msleep(50);

	/* Deassert reset (both GPIOs LOW) */
	gpiod_set_value_cansleep(reset->gpio75, 0);
	gpiod_set_value_cansleep(reset->gpio77, 0);
	dev_info(dev, "PHY reset deasserted (GPIO75=LOW, GPIO77=LOW)\n");

	/* Wait 100ms for PHYs to initialize before SSDK loads */
	msleep(100);

	dev_info(dev, "PHY reset sequence complete\n");

	platform_set_drvdata(pdev, reset);

	/* Keep driver loaded so GPIOs stay claimed */
	return 0;
}

static void calix_phy_reset_remove(struct platform_device *pdev)
{
	dev_info(&pdev->dev, "Driver removed\n");
	/* GPIOs auto-released by devm_* */
}

static const struct of_device_id calix_phy_reset_of_match[] = {
	{ .compatible = "calix,gs2028e-phy-reset" },
	{ }
};
MODULE_DEVICE_TABLE(of, calix_phy_reset_of_match);

static struct platform_driver calix_phy_reset_driver = {
	.driver = {
		.name = "calix-phy-reset",
		.of_match_table = calix_phy_reset_of_match,
	},
	.probe = calix_phy_reset_probe,
	.remove = calix_phy_reset_remove,
};

/*
 * Register driver early (arch_initcall) so it's ready when
 * device tree is processed, but actual probe happens later
 * when GPIO controller is available
 */
static int __init calix_phy_reset_init(void)
{
	return platform_driver_register(&calix_phy_reset_driver);
}
arch_initcall(calix_phy_reset_init);

static void __exit calix_phy_reset_exit(void)
{
	platform_driver_unregister(&calix_phy_reset_driver);
}
module_exit(calix_phy_reset_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Brendan Cook");
MODULE_DESCRIPTION("Calix GS2028E PHY Reset Platform Driver");
