using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;
using System.IO.Ports;
using System.Windows.Forms.DataVisualization.Charting;
using static System.Windows.Forms.VisualStyles.VisualStyleElement;

namespace GUI
{
    public partial class Form1 : Form
    {
        double setpointTemperature;
        public Form1()
        {
            InitializeComponent();
            serialPort1.DataReceived += new SerialDataReceivedEventHandler(DataReceivedHandler);
        }

        private void Form1_Load(object sender, EventArgs e)
        {
            string[] ports = SerialPort.GetPortNames();
            cBoxCOMPORT.Items.AddRange(ports);

            btnConnect.Enabled = true;
            btnDisconnect.Enabled = false;
        }

        private void btnConnect_Click(object sender, EventArgs e)
        {
            try {
                serialPort1.PortName = cBoxCOMPORT.Text;
                serialPort1.BaudRate = Convert.ToInt32(cBoxBaudRate.Text);
                serialPort1.DataBits = Convert.ToInt32(cBoxDataBits.Text);
                serialPort1.StopBits = (StopBits)Enum.Parse(typeof(StopBits), cBoxStopBits.Text);
                serialPort1.Parity = (Parity)Enum.Parse(typeof(Parity), cBoxParityBits.Text);

                serialPort1.Open();
                progressBar1.Value = 100;
                btnConnect.Enabled = false;
                btnDisconnect.Enabled = true;

            } catch (Exception err){
                MessageBox.Show(err.Message, "Error during connect!", MessageBoxButtons.OK, MessageBoxIcon.Error);
                btnConnect.Enabled = true;
                btnDisconnect.Enabled = false;
            }
        }

        private void btnDisconnect_Click(object sender, EventArgs e)
        {
            if (serialPort1.IsOpen)
            {
                serialPort1.Close();
                progressBar1.Value = 0;
                btnConnect.Enabled = true;
                btnDisconnect.Enabled = false;
            }
        }

        private void DataReceivedHandler(object sender, SerialDataReceivedEventArgs e)
        {
            SerialPort sp = (SerialPort)sender;
            string dataIn = sp.ReadLine().Replace(";", string.Empty);
            double temperature;

            // wyświetlanie aktualnej temperatury
            lblCurrentTemperature.Text = dataIn;

            if (double.TryParse(dataIn, out temperature))
            {
                UpdateChart(temperature);
            }
        }

        // aktualizacja wykresu
        private void UpdateChart(double temperature)
        {
            this.Invoke((MethodInvoker)delegate
            {
                // temperatura odczytana
                chart1.Series[0].Points.AddY(temperature);
                // temperatura zadana
                chart1.Series[1].Points.AddY(setpointTemperature);

            });
        }

        // ustawianie temperatury zadanej
        private void btnSetTemperature_Click(object sender, EventArgs e)
        {
            if (serialPort1.IsOpen)
            {
                if (cBoxSetpointTemperature.Text == "")
                {
                    MessageBox.Show("Pole nie może być puste. Wprowadź wartość temperatury.", "Błąd", MessageBoxButtons.OK, MessageBoxIcon.Error);
                }
                else 
                { 
                    setpointTemperature = Convert.ToDouble(cBoxSetpointTemperature.Text.Replace('.', ','));
                    
                    if (setpointTemperature < 0.0 || setpointTemperature > 75.0)
                    {
                        MessageBox.Show("Wprowadź liczbę z zakresu od 0 do 75.", "Błąd", MessageBoxButtons.OK, MessageBoxIcon.Warning);
                        cBoxSetpointTemperature.Text = "0.0";
                    }
                    else
                    {
                        string temperatureToSet = string.Format("{{setTemperature:{0:F2} }}", setpointTemperature).Replace(',', '.').Replace(" ", string.Empty);
                        serialPort1.WriteLine(temperatureToSet);
                        Console.WriteLine(temperatureToSet);
                    }

                }

            }
        }

        // ustawianie parametrów regulatora
        private void btnSetPidSettings_Click(object sender, EventArgs e)
        {
            if (serialPort1.IsOpen)
            {
                if (cBoxKp.Text == "" || cBoxKi.Text == "" || cBoxKd.Text == "")
                {
                    MessageBox.Show("Pola nie mogą być puste. Wprowadź wszystkie wartości nastaw regulatora PID.", "Błąd", MessageBoxButtons.OK, MessageBoxIcon.Error);
                }
                else 
                {
                    string pidSettings = string.Format("{{kp:{0:F2};ki:{1:F2};kd:{2:F2} }}", Convert.ToDouble(cBoxKp.Text.Replace('.', ',')), Convert.ToDouble(cBoxKi.Text.Replace('.', ',')), Convert.ToDouble(cBoxKd.Text.Replace('.', ','))).Replace(',', '.').Replace(';', ',').Replace(" ", string.Empty);
                    serialPort1.WriteLine(pidSettings);
                    Console.WriteLine(pidSettings.Trim());

                }
            }
        }
    }
}
