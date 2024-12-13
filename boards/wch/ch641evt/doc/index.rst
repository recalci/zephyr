.. zephyr:board:: ch641evt

Overview
********

The `WCH`_ CH641EVT hardware provides support for QingKe 32-bit RISC-V2A processor
and the following devices:

* CLOCK
* :abbr:`GPIO (General Purpose Input Output)`
* :abbr:`NVIC (Nested Vectored Interrupt Controller)`

The `WCH webpage on CH641`_ contains the processor's information and the datasheet.

Hardware
********

.. image:: ch641evt.jpg
     :align: center
     :alt: CH641 Evaluation Board

Supported Features
==================

The ``ch641evt`` board target supports the following hardware features:

+-----------+------------+----------------------+
| Interface | Controller | Driver/Component     |
+===========+============+======================+
| CLOCK     | on-chip    | clock_control        |
+-----------+------------+----------------------+
| GPIO      | on-chip    | gpio                 |
+-----------+------------+----------------------+
| PWM       | on-chip    | pwm                  |
+-----------+------------+----------------------+
| PINCTRL   | on-chip    | pinctrl              |
+-----------+------------+----------------------+
| TIMER     | on-chip    | timer                |
+-----------+------------+----------------------+
| UART      | on-chip    | uart                 |
+-----------+------------+----------------------+

Other hardware features have not been enabled yet for this board.

Connections and IOs
===================

LED
---

* LED1 (red)  = PB1 (active low)
* LED2 (blue) = PA6 (active low)

Programming and Debugging
*************************

Applications for the ``ch641evt`` board target can be built and flashed in the
usual way (see :ref:`build_an_application` and :ref:`application_run` for more
details); however, an external programmer is required since the board does not
have any built-in debug support.

The following pins of the external programmer must be connected to the following
pins that leads to the USB-C:

* VCC = VBUS (5V)
* GND = GND
* SWIO = CC1 (PB0)

CC1 (PB0) is a SWDIO port. After mounting, the CC1 pin that leads to the USB-C
can be upgraded through Link.

Flashing
========

You can use ``minichlink`` to flash the board. Once ``minichlink`` has been set
up, build and flash applications as usual (see :ref:`build_an_application` and
:ref:`application_run` for more details).

Here is an example for the :zephyr:code-sample:`blinky` application.

.. zephyr-app-commands::
   :zephyr-app: samples/basic/blinky
   :board: ch641evt
   :goals: build flash

Debugging
=========

This board can be debugged via OpenOCD or ``minichlink``.

Testing the LED on the WCH CH641EVT
***********************************

There is 1 sample program that allow you to test that the LED on the board is
working properly with Zephyr:

.. code-block:: console

   samples/basic/blinky

You can build and flash the examples to make sure Zephyr is running
correctly on your board. The button and LED definitions can be found
in :zephyr_file:`boards/wch/ch641evt/ch641evt.dts`.

References
**********

.. target-notes::

.. _WCH: http://www.wch-ic.com
.. _WCH webpage on CH641: https://www.wch-ic.com/products/CH641.html
