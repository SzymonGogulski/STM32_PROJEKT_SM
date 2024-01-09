using System;
using System.Windows.Forms;
using System.IO.Ports;

namespace GUI
{
    public partial class Form1 : Form
    {
        double setpointTemperature;
        public Form1()
        {
            InitializeComponent();
            serialPort1.DataReceived += new SerialDataReceivedEventHandler(DataReceivedHandler);
            this.FormClosing += new FormClosingEventHandler(Form1_FormClosing);
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
            string[] dataIn = sp.ReadLine().Replace(";", string.Empty).Split(',');
            double temp, tempRef;
            int U;

            // przeniesienie aktualizacji interfejsu użytkownika na wątek UI
            this.Invoke((MethodInvoker)delegate
            {
                if (double.TryParse(dataIn[0].Replace('.', ','), out temp) &&
                    double.TryParse(dataIn[1].Replace('.', ','), out tempRef) &&
                    int.TryParse(dataIn[2].Replace('.', ','), out U))
                {
                    UpdateChart(temp, tempRef, U);
                    // wyświetlanie aktualnej temperatury
                    lblCurrentTemperature.Text = dataIn[0];
                }
                else
                {
                    MessageBox.Show("Błąd aktualizowania wykresu!", "Błąd", MessageBoxButtons.OK, MessageBoxIcon.Error);

                }
            });
        }

        // aktualizacja wykresu
        private void UpdateChart(double temperature, double refTemperature, int U)
        {
            this.Invoke((MethodInvoker)delegate
            {
                // temperatura odczytana
                chart1.Series[0].Points.AddY(temperature);
                // temperatura zadana
                chart1.Series[1].Points.AddY(refTemperature);
                // sygnał sterujący
                chart1.Series[2].Points.AddY(U);

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
                    
                    if (setpointTemperature < 25.0 || setpointTemperature > 75.0)
                    {
                        MessageBox.Show("Wprowadź liczbę z zakresu od 25 do 75.", "Błąd", MessageBoxButtons.OK, MessageBoxIcon.Warning);
                        cBoxSetpointTemperature.Text = "0.0";
                    }
                    else
                    {
                        string temperatureToSet = string.Format("t{0:F2};", setpointTemperature).Replace(',', '.');
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
                    string kp = cBoxKp.Text.Replace('.', ',');
                    string ki = cBoxKi.Text.Replace('.', ',');
                    string kd = cBoxKd.Text.Replace('.', ',');
                    string pidSettings = string.Format("p{0:F4}:{1:F4}:{2:F4};", Convert.ToDouble(kp), Convert.ToDouble(ki), Convert.ToDouble(kd)).Replace(',', '.').Replace(':', ',');
                    serialPort1.WriteLine(pidSettings);
                    Console.WriteLine(pidSettings.Trim());
                    txtKp.Text = string.Format("{0:F4}", Convert.ToDouble(kp));
                    txtKi.Text = string.Format("{0:F4}", Convert.ToDouble(ki));
                    txtKd.Text = string.Format("{0:F4}", Convert.ToDouble(kd));

                }
            }
        }
        private void btnClearChart_Click(object sender, EventArgs e)
        {
            if (serialPort1.IsOpen)
            {
                this.Invoke((MethodInvoker)delegate
            {
                // czyszczenie punktów na wykresie
                chart1.Series[0].Points.Clear();
                chart1.Series[1].Points.Clear();
                chart1.Series[2].Points.Clear();
            });
            }
        }
        private void Form1_FormClosing(object sender, FormClosingEventArgs e)
        {
            if (serialPort1.IsOpen)
            {
                serialPort1.Close();
            }
        }
    }
}
